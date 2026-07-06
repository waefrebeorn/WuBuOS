/*
 * wubu_image_parse.c  --  WuBuOS WuBuFile Parser
 *
 * Extracted from wubu_image.c (2026-07-06): all WuBuFile parsing
 * logic moved here. The original wubu_image.c had the parser inline
 * (~293 lines) before the layer cache, tar writer, build engine,
 * and manifest operations. This module is self-contained: it only
 * depends on wubu_image_internal.h (types + string helpers) and
 * the public wubu_arch_from_string / wubu_os_from_string API.
 *
 * C11 only. No globals. No god headers.
 */

#include "wubu_image_internal.h"

/* -- Instruction Parser --------------------------------------------- */

int parse_instruction(const char *line, WubuInstruction *inst, int line_num) {
    char *trimmed = str_dup(line);
    str_trim(trimmed);

    if (!*trimmed || *trimmed == '#') {
        free(trimmed);
        return 0;  /* Skip empty/comment */
    }

    inst->line_num = line_num;
    inst->has_json_form = false;
    strcpy(inst->original, line);

    /* Parse instruction type */
    char *token = strtok(trimmed, " \t");
    if (!token) {
        free(trimmed);
        return -1;
    }

    /* Convert to uppercase for comparison */
    char upper[32];
    for (int i = 0; token[i] && i < 31; i++) upper[i] = toupper(token[i]);
    upper[31] = '\0';

    if (strcmp(upper, "FROM") == 0) inst->type = WUBU_INST_FROM;
    else if (strcmp(upper, "RUN") == 0) inst->type = WUBU_INST_RUN;
    else if (strcmp(upper, "COPY") == 0) inst->type = WUBU_INST_COPY;
    else if (strcmp(upper, "ADD") == 0) inst->type = WUBU_INST_ADD;
    else if (strcmp(upper, "CMD") == 0) inst->type = WUBU_INST_CMD;
    else if (strcmp(upper, "ENTRYPOINT") == 0) inst->type = WUBU_INST_ENTRYPOINT;
    else if (strcmp(upper, "ENV") == 0) inst->type = WUBU_INST_ENV;
    else if (strcmp(upper, "ARG") == 0) inst->type = WUBU_INST_ARG;
    else if (strcmp(upper, "WORKDIR") == 0) inst->type = WUBU_INST_WORKDIR;
    else if (strcmp(upper, "USER") == 0) inst->type = WUBU_INST_USER;
    else if (strcmp(upper, "EXPOSE") == 0) inst->type = WUBU_INST_EXPOSE;
    else if (strcmp(upper, "VOLUME") == 0) inst->type = WUBU_INST_VOLUME;
    else if (strcmp(upper, "LABEL") == 0) inst->type = WUBU_INST_LABEL;
    else if (strcmp(upper, "ONBUILD") == 0) inst->type = WUBU_INST_ONBUILD;
    else if (strcmp(upper, "HEALTHCHECK") == 0) inst->type = WUBU_INST_HEALTHCHECK;
    else if (strcmp(upper, "SHELL") == 0) inst->type = WUBU_INST_SHELL;
    else if (strcmp(upper, "MOUNT") == 0) inst->type = WUBU_INST_MOUNT;
    else if (strcmp(upper, "DEVICE") == 0) inst->type = WUBU_INST_DEVICE;
    else if (strcmp(upper, "SECURITY") == 0) inst->type = WUBU_INST_SECURITY;
    else if (strcmp(upper, "STOPSIGNAL") == 0) inst->type = WUBU_INST_STOP_SIGNAL;
    else if (strcmp(upper, "ARCH") == 0) inst->type = WUBU_INST_ARCH;
    else if (strcmp(upper, "OS") == 0) inst->type = WUBU_INST_OS;
    else {
        fprintf(stderr, "Line %d: Unknown instruction: %s\n", line_num, token);
        free(trimmed);
        return -1;
    }

    /* Get rest of line as args */
    char *rest = strtok(NULL, "");
    if (rest) {
        str_trim(rest);
        strncpy(inst->args, rest, WUBU_MAX_CMD_LEN - 1);
        inst->args[WUBU_MAX_CMD_LEN - 1] = '\0';

        /* Check for JSON form: ["cmd", "arg"] */
        if (rest[0] == '[' && rest[strlen(rest) - 1] == ']') {
            inst->has_json_form = true;
        }
    } else {
        inst->args[0] = '\0';
    }

    free(trimmed);
    return 1;
}

/* -- WuBuFile Parser ------------------------------------------------ */

int wubu_parse_wubufile(const char *path, WubuBuildContext *ctx) {
    if (!path || !ctx) return -1;

    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        return -1;
    }

    strncpy(ctx->wubufile_path, path, WUBU_MAX_PATH - 1);

    char line[4096];
    int line_num = 0;
    int current_stage = 0;
    WubuStage *stage = &ctx->stages[0];
    memset(stage, 0, sizeof(WubuStage));
    stage->arch = WUBU_ARCH_X86_64;
    stage->os = WUBU_OS_LINUX;
    strcpy(stage->shell, "/bin/bash");

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        char *trimmed = str_dup(line);
        str_trim(trimmed);

        if (!*trimmed || *trimmed == '#') {
            free(trimmed);
            continue;
        }

        /* Handle line continuations */
        size_t len = strlen(trimmed);
        while (len > 0 && trimmed[len - 1] == '\\') {
            trimmed[len - 1] = '\0';
            char next_line[4096];
            if (!fgets(next_line, sizeof(next_line), f)) break;
            line_num++;
            char *next_trimmed = str_dup(next_line);
            str_trim(next_trimmed);
            char combined[8192];
            snprintf(combined, sizeof(combined), "%s%s", trimmed, next_trimmed);
            free(trimmed);
            free(next_trimmed);
            trimmed = str_dup(combined);
            len = strlen(trimmed);
        }

        WubuInstruction inst;
        int result = parse_instruction(trimmed, &inst, line_num);
        free(trimmed);

        if (result <= 0) {
            if (result < 0) { fclose(f); return -1; }
            continue;
        }

        /* Handle FROM - new stage */
        if (inst.type == WUBU_INST_FROM) {
            if (stage->inst_count > 0 || current_stage > 0) {
                current_stage++;
                if (current_stage >= WUBU_MAX_STAGES) {
                    fprintf(stderr, "Too many stages (max %d)\n", WUBU_MAX_STAGES);
                    fclose(f);
                    return -1;
                }
                stage = &ctx->stages[current_stage];
                memset(stage, 0, sizeof(WubuStage));
                stage->arch = WUBU_ARCH_X86_64;
                stage->os = WUBU_OS_LINUX;
                strcpy(stage->shell, "/bin/bash");
            }

            /* Parse FROM args: <image>[:tag] [AS <name>] */
            char *img = strtok(inst.args, " \t");
            if (img) {
                char *tag = strchr(img, ':');
                if (tag) {
                    *tag = '\0';
                    tag++;
                    strncpy(stage->base_tag, tag, 31);
                }
                strncpy(stage->base_image, img, 127);

                char *as = strtok(NULL, " \t");
                if (as && strcmp(as, "AS") == 0) {
                    as = strtok(NULL, " \t");
                    if (as) strncpy(stage->name, as, 63);
                }
            }
            continue;
        }

        /* Add instruction to current stage */
        if (stage->inst_count >= WUBU_MAX_INSTRUCTIONS) {
            fprintf(stderr, "Too many instructions in stage (max %d)\n", WUBU_MAX_INSTRUCTIONS);
            fclose(f);
            return -1;
        }
        inst.stage = current_stage;
        stage->insts[stage->inst_count++] = inst;

        /* Handle other instructions that affect stage config */
        switch (inst.type) {
            case WUBU_INST_ENV: {
                /* ENV KEY=VAL KEY2=VAL2 ... */
                char *kv = strtok(inst.args, " \t");
                while (kv && stage->env_count < WUBU_MAX_ENVS) {
                    strncpy(stage->envs[stage->env_count], kv, 127);
                    stage->env_count++;
                    kv = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_VOLUME: {
                /* VOLUME /path /path2 ... */
                char *vol = strtok(inst.args, " \t");
                while (vol && stage->volume_count < WUBU_MAX_VOLUMES) {
                    strncpy(stage->volumes[stage->volume_count], vol, 255);
                    stage->volume_count++;
                    vol = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_LABEL: {
                /* LABEL KEY=VAL KEY2=VAL2 ... */
                char *lkv = strtok(inst.args, " \t");
                while (lkv && stage->env_count < WUBU_MAX_LABELS) {
                    strncpy(stage->labels[stage->env_count], lkv, 127);
                    stage->env_count++;
                    lkv = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_EXPOSE: {
                /* EXPOSE 80/tcp 443/tcp ... */
                char *port = strtok(inst.args, " \t");
                while (port && stage->port_count < WUBU_MAX_PORTS) {
                    strncpy(stage->ports[stage->port_count], port, 31);
                    stage->port_count++;
                    port = strtok(NULL, " \t");
                }
                break;
            }
            case WUBU_INST_WORKDIR: {
                str_trim(inst.args);
                strncpy(stage->workdir, inst.args, 255);
                break;
            }
            case WUBU_INST_USER: {
                str_trim(inst.args);
                char *colon = strchr(inst.args, ':');
                if (colon) {
                    *colon = '\0';
                    strncpy(stage->user, inst.args, 63);
                    strncpy(stage->group, colon + 1, 63);
                } else {
                    strncpy(stage->user, inst.args, 63);
                }
                break;
            }
            case WUBU_INST_ENTRYPOINT: {
                strncpy(stage->entrypoint, inst.args, WUBU_MAX_CMD_LEN - 1);
                break;
            }
            case WUBU_INST_CMD: {
                strncpy(stage->cmd, inst.args, WUBU_MAX_CMD_LEN - 1);
                break;
            }
            case WUBU_INST_SHELL: {
                str_trim(inst.args);
                strncpy(stage->shell, inst.args, 127);
                break;
            }
            case WUBU_INST_ARCH: {
                stage->arch = wubu_arch_from_string(inst.args);
                break;
            }
            case WUBU_INST_OS: {
                stage->os = wubu_os_from_string(inst.args);
                break;
            }
            default:
                break;
        }
    }

    fclose(f);
    ctx->stage_count = current_stage + 1;

    /* Set context path from WuBuFile location */
    char *slash = strrchr(ctx->wubufile_path, '/');
    if (slash) {
        *slash = '\0';
        strncpy(ctx->context_path, ctx->wubufile_path, WUBU_MAX_PATH - 1);
        *slash = '/';
    } else {
        strcpy(ctx->context_path, ".");
    }

    return 0;
}

int wubu_parse_wubufile_str(const char *content, WubuBuildContext *ctx) {
    if (!content || !ctx) return -1;
    /* Write to temp file and parse */
    char tmp_path[WUBU_MAX_PATH];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/wubufile_%d", getpid());
    FILE *f = fopen(tmp_path, "w");
    if (!f) return -1;
    fputs(content, f);
    fclose(f);
    int ret = wubu_parse_wubufile(tmp_path, ctx);
    unlink(tmp_path);
    return ret;
}