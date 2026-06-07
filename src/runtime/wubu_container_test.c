/*
 * wubu_container_test.c — WuBuOS .wubu Container Format Test Suite
 */

#include "wubu_container.h"
#include "wubu_exec.h"
#include "../compiler/holyc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_run = 0, g_pass = 0;

#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s\n", msg); } \
} while(0)

int main(void) {
    printf("═══ WuBuOS .wubu Container Test Suite ═══\n\n");

    /* ── Container Create & Parse ── */
    printf("[Container Create/Parse]\n");
    {
        WUBU_HEADER hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.payload_type = WUBU_PAYLOAD_HOLYC_SRC;
        hdr.arch = WUBU_ARCH_X86_64;
        hdr.flags = WUBU_FLAG_JIT_COMPILE;
        hdr.handler_id = 2; /* HolyC JIT */
        hdr.os_persona = WUBU_OS_NATIVE;
        hdr.entry_offset = 0;

        const char *payload = "return 42;";
        uint8_t buf[512];
        size_t out_size;

        int rc = wubu_container_create(&hdr, payload, strlen(payload),
                                       buf, sizeof(buf), &out_size);
        T(rc == 0, "container create");
        T(out_size == WUBU_HEADER_SIZE + strlen(payload), "container size");

        /* Parse it back */
        WUBU_HEADER parsed;
        const void *pld;
        size_t pld_size;
        rc = wubu_container_parse(buf, out_size, &parsed, &pld, &pld_size);
        T(rc == 0, "container parse");
        T(parsed.payload_type == WUBU_PAYLOAD_HOLYC_SRC, "payload type preserved");
        T(parsed.arch == WUBU_ARCH_X86_64, "arch preserved");
        T(parsed.handler_id == 2, "handler_id preserved");
        T(pld_size == strlen(payload), "payload size preserved");
        T(memcmp(pld, payload, pld_size) == 0, "payload data preserved");
    }

    /* ── Container Validate ── */
    printf("\n[Container Validate]\n");
    {
        WUBU_HEADER hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.payload_type = WUBU_PAYLOAD_NATIVE_EXEC;
        hdr.arch = WUBU_ARCH_X86_64;
        hdr.handler_id = 1;
        hdr.os_persona = WUBU_OS_NATIVE;

        uint8_t buf[256];
        size_t out_size;
        wubu_container_create(&hdr, "test", 4, buf, sizeof(buf), &out_size);

        T(wubu_container_validate(buf, out_size) == 0, "valid container");

        /* Corrupt magic */
        buf[0] = 'X';
        T(wubu_container_validate(buf, out_size) != 0, "corrupted magic rejected");
    }

    /* ── Payload Detection ── */
    printf("\n[Payload Detection]\n");
    {
        /* ELF */
        uint8_t elf[] = {0x7F, 'E', 'L', 'F', 0x02, 0x01, 0x01, 0x00};
        T(wubu_detect_payload_type(elf, sizeof(elf)) == WUBU_PAYLOAD_LINUX_ELF,
          "detect ELF");

        /* PE */
        uint8_t pe[] = {'M', 'Z', 0x90, 0x00, 0x03, 0x00, 0x00, 0x00};
        T(wubu_detect_payload_type(pe, sizeof(pe)) == WUBU_PAYLOAD_WIN_PE,
          "detect PE");

        /* WASM */
        uint8_t wasm[] = {0x00, 'a', 's', 'm', 0x01, 0x00, 0x00, 0x00};
        T(wubu_detect_payload_type(wasm, sizeof(wasm)) == WUBU_PAYLOAD_WASM,
          "detect WASM");

        /* Shebang */
        uint8_t sh[] = "#!/bin/bash\necho hello";
        T(wubu_detect_payload_type(sh, sizeof(sh)) == WUBU_PAYLOAD_SHELL_SCRIPT,
          "detect shell script");

        /* Python shebang */
        uint8_t py[] = "#!/usr/bin/python3\nprint('hi')";
        T(wubu_detect_payload_type(py, sizeof(py)) == WUBU_PAYLOAD_PYTHON,
          "detect python script");

        /* HolyC */
        uint8_t hc[] = "U0 main() { return 0; }";
        T(wubu_detect_payload_type(hc, sizeof(hc)) == WUBU_PAYLOAD_HOLYC_SRC,
          "detect HolyC source");

        /* C source */
        uint8_t c[] = "#include <stdio.h>\nint main() { return 0; }";
        T(wubu_detect_payload_type(c, sizeof(c)) == WUBU_PAYLOAD_C_SRC,
          "detect C source");

        /* .wubu container */
        WUBU_HEADER hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.payload_type = WUBU_PAYLOAD_HOLYC_SRC;
        hdr.arch = WUBU_ARCH_X86_64;
        hdr.handler_id = 2;
        hdr.os_persona = WUBU_OS_NATIVE;
        uint8_t wubu_buf[256];
        size_t wubu_size;
        wubu_container_create(&hdr, "test", 4, wubu_buf, sizeof(wubu_buf), &wubu_size);
        T(wubu_detect_payload_type(wubu_buf, wubu_size) == WUBU_PAYLOAD_NESTED_WUBU,
          "detect .wubu container");

        /* Unknown */
        uint8_t unk[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        T(wubu_detect_payload_type(unk, sizeof(unk)) == WUBU_PAYLOAD_DATA,
          "detect unknown as data");
    }

    /* ── Convenience Wrappers ── */
    printf("\n[Convenience Wrappers]\n");
    {
        uint8_t buf[512];
        size_t out_size;

        int rc = wubu_container_native_exec("code", 4, 0,
                                            buf, sizeof(buf), &out_size);
        T(rc == 0, "native exec container");
        T(wubu_container_validate(buf, out_size) == 0, "native exec valid");

        rc = wubu_container_linux_elf("elf", 3, buf, sizeof(buf), &out_size);
        T(rc == 0, "linux elf container");
        T(wubu_container_validate(buf, out_size) == 0, "linux elf valid");
    }

    /* ── Format Detection (wubu_exec) ── */
    printf("\n[Format Detection (wubu_exec)]\n");
    {
        bool is_wubu = false;

        uint8_t elf[] = {0x7F, 'E', 'L', 'F'};
        WUBU_PAYLOAD_TYPE t = wubu_detect_format(elf, sizeof(elf), &is_wubu);
        T(t == WUBU_PAYLOAD_LINUX_ELF && !is_wubu, "detect ELF via wubu_detect_format");

        uint8_t sh[] = "#!/bin/sh";
        t = wubu_detect_format(sh, sizeof(sh), &is_wubu);
        T(t == WUBU_PAYLOAD_SHELL_SCRIPT && !is_wubu, "detect shell via wubu_detect_format");

        /* .wubu container */
        WUBU_HEADER hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.payload_type = WUBU_PAYLOAD_HOLYC_SRC;
        hdr.arch = WUBU_ARCH_X86_64;
        hdr.handler_id = 2;
        hdr.os_persona = WUBU_OS_NATIVE;
        uint8_t wubu_buf[256];
        size_t wubu_size;
        wubu_container_create(&hdr, "test", 4, wubu_buf, sizeof(wubu_buf), &wubu_size);

        t = wubu_detect_format(wubu_buf, wubu_size, &is_wubu);
        T(t == WUBU_PAYLOAD_HOLYC_SRC && is_wubu, "detect .wubu HolyC container");
    }

    /* ── Payload Type Names ── */
    printf("\n[Payload Type Names]\n");
    {
        T(strcmp(wubu_payload_name(WUBU_PAYLOAD_NATIVE_EXEC), "WuBuOS Native Executable") == 0,
          "name: native exec");
        T(strcmp(wubu_payload_name(WUBU_PAYLOAD_LINUX_ELF), "Linux ELF (VSL)") == 0,
          "name: linux elf");
        T(strcmp(wubu_payload_name(WUBU_PAYLOAD_WIN_PE), "Windows PE (Proton)") == 0,
          "name: win pe");
        T(strcmp(wubu_payload_name(WUBU_PAYLOAD_HOLYC_SRC), "HolyC Source") == 0,
          "name: holyc");
    }

    /* ── VSL Lifecycle ── */
    printf("\n[VSL Lifecycle]\n");
    {
        T(!wubu_vsl_active(), "VSL not active initially");
        T(wubu_vsl_init() == 0, "VSL init");
        T(wubu_vsl_active(), "VSL active after init");
        wubu_vsl_shutdown();
        T(!wubu_vsl_active(), "VSL not active after shutdown");
    }

    /* ── End-to-End: HolyC in .wubu ── */
    printf("\n[E2E: HolyC in .wubu]\n");
    {
        /* Create a .wubu container with HolyC source */
        const char *hc_src = "return 2 + 3;";
        WUBU_HEADER hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.payload_type = WUBU_PAYLOAD_HOLYC_SRC;
        hdr.arch = WUBU_ARCH_X86_64;
        hdr.flags = WUBU_FLAG_JIT_COMPILE;
        hdr.handler_id = 2;
        hdr.os_persona = WUBU_OS_NATIVE;

        uint8_t buf[512];
        size_t out_size;
        wubu_container_create(&hdr, hc_src, strlen(hc_src),
                              buf, sizeof(buf), &out_size);

        /* Parse and execute */
        WUBU_HEADER parsed;
        const void *payload;
        size_t payload_size;
        wubu_container_parse(buf, out_size, &parsed, &payload, &payload_size);

        T(parsed.payload_type == WUBU_PAYLOAD_HOLYC_SRC, "parsed HolyC container");

        /* Execute the HolyC payload */
        int64_t result = wubu_exec_holyc((const char *)payload, payload_size);
        T(result == 5, "HolyC 2+3 = 5 via .wubu");
    }

    /* ── End-to-End: Universal Exec ── */
    printf("\n[E2E: Universal Exec]\n");
    {
        /* HolyC source file */
        int64_t r = wubu_exec("return 7 * 6;", 13, "test.hc");
        T(r == 42, "universal exec HolyC: 7*6 = 42");

        /* .wubu container with HolyC */
        const char *hc = "return 100 / 4;";
        WUBU_HEADER hdr;
        memset(&hdr, 0, sizeof(hdr));
        hdr.payload_type = WUBU_PAYLOAD_HOLYC_SRC;
        hdr.arch = WUBU_ARCH_X86_64;
        hdr.handler_id = 2;
        hdr.os_persona = WUBU_OS_NATIVE;
        uint8_t buf[512];
        size_t out_size;
        wubu_container_create(&hdr, hc, strlen(hc), buf, sizeof(buf), &out_size);

        r = wubu_exec(buf, out_size, "test.wubu");
        T(r == 25, "universal exec .wubu: 100/4 = 25");
    }

    printf("\n═══ Results: %d/%d passed ═══\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
