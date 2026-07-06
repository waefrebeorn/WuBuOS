/*
 * oci_http_client.c  --  HTTP/TLS Client for OCI Registry Operations
 * 
 * Extracted from wubu_oci.c (lines 51-355).
 * Supports plain HTTP and HTTPS via mbedTLS (optional).
 */

#include "oci_internal.h"

/* -- HTTP Client Implementation -------------------------------------- */

int oci_http_connect(OciHttpClient *client, const char *host, int port, bool use_tls) {
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0) return -1;

    int sockfd = -1;
    for (struct addrinfo *rp = res; rp; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) continue;
        struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(res);

    if (sockfd < 0) return -1;

    client->sockfd = sockfd;
    strncpy(client->host, host, sizeof(client->host) - 1);
    client->port = port;
    client->use_tls = use_tls;

    /* Initialize TLS if requested and mbedTLS is available */
#ifdef MBEDTLS_SSL_H
    client->tls_initialized = false;
    if (use_tls) {
        /* Initialize mbedTLS structures */
        mbedtls_ssl_init(&client->ssl);
        mbedtls_ssl_config_init(&client->conf);
        mbedtls_entropy_init(&client->entropy);
        mbedtls_ctr_drbg_init(&client->ctr_drbg);
        mbedtls_x509_crt_init(&client->cacert);

        /* Seed the RNG */
        rc = mbedtls_ctr_drbg_seed(&client->ctr_drbg, mbedtls_entropy_func, &client->entropy,
                                   NULL, 0);
        if (rc != 0) {
            fprintf(stderr, "[wubu_oci] mbedtls_ctr_drbg_seed failed: %d\n", rc);
            goto tls_fail;
        }

        /* Load default trusted CA certificates (system store not available, use none) */
        /* In production, you would load CA certs here:
         * rc = mbedtls_x509_crt_parse_file(&client->cacert, "/etc/ssl/certs/ca-certificates.crt");
         */
        /* For now, we skip certificate verification (insecure but functional) */
        mbedtls_ssl_conf_authmode(&client->conf, MBEDTLS_SSL_VERIFY_NONE);

        /* Set up SSL config */
        rc = mbedtls_ssl_config_defaults(&client->conf,
                                         MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT);
        if (rc != 0) {
            fprintf(stderr, "[wubu_oci] mbedtls_ssl_config_defaults failed: %d\n", rc);
            goto tls_fail;
        }

        mbedtls_ssl_conf_rng(&client->conf, mbedtls_ctr_drbg_random, &client->ctr_drbg);
        mbedtls_ssl_conf_ca_chain(&client->conf, &client->cacert, NULL);

        /* Set hostname for SNI */
        rc = mbedtls_ssl_set_hostname(&client->ssl, host);
        if (rc != 0) {
            fprintf(stderr, "[wubu_oci] mbedtls_ssl_set_hostname failed: %d\n", rc);
            goto tls_fail;
        }

        /* Apply config to SSL context */
        rc = mbedtls_ssl_setup(&client->ssl, &client->conf);
        if (rc != 0) {
            fprintf(stderr, "[wubu_oci] mbedtls_ssl_setup failed: %d\n", rc);
            goto tls_fail;
        }

        /* Set bio callbacks for socket */
        mbedtls_ssl_set_bio(&client->ssl, &client->sockfd,
                            mbedtls_net_send, mbedtls_net_recv, NULL);

        /* Perform TLS handshake */
        while ((rc = mbedtls_ssl_handshake(&client->ssl)) != 0) {
            if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) {
                fprintf(stderr, "[wubu_oci] mbedtls_ssl_handshake failed: %d\n", rc);
                goto tls_fail;
            }
        }

        client->tls_initialized = true;
    }
#else
    /* Note: TLS support would require OpenSSL/mbedTLS integration.
     * For now, we support plain HTTP. HTTPS registries need a TLS library. */
    if (use_tls) {
        close(sockfd);
        return -1; /* TLS not implemented - compile with -DMBEDTLS_SSL_H */
    }
#endif

    return 0;

#ifdef MBEDTLS_SSL_H
tls_fail:
    mbedtls_ssl_free(&client->ssl);
    mbedtls_ssl_config_free(&client->conf);
    mbedtls_entropy_free(&client->entropy);
    mbedtls_ctr_drbg_free(&client->ctr_drbg);
    mbedtls_x509_crt_free(&client->cacert);
    client->tls_initialized = false;
    close(sockfd);
    client->sockfd = -1;
    return -1;
#endif
}

void oci_http_close(OciHttpClient *client) {
    if (client->sockfd >= 0) {
#ifdef MBEDTLS_SSL_H
        if (client->use_tls && client->tls_initialized) {
            mbedtls_ssl_close_notify(&client->ssl);
            mbedtls_ssl_free(&client->ssl);
            mbedtls_ssl_config_free(&client->conf);
            mbedtls_entropy_free(&client->entropy);
            mbedtls_ctr_drbg_free(&client->ctr_drbg);
            mbedtls_x509_crt_free(&client->cacert);
            client->tls_initialized = false;
        }
#endif
        close(client->sockfd);
        client->sockfd = -1;
    }
}

int oci_http_send(OciHttpClient *client, const void *data, size_t len) {
    const char *p = (const char *)data;
    size_t sent = 0;

#ifdef MBEDTLS_SSL_H
    if (client->use_tls && client->tls_initialized) {
        while (sent < len) {
            int n = mbedtls_ssl_write(&client->ssl, (const unsigned char *)(p + sent), len - sent);
            if (n <= 0) {
                if (n != MBEDTLS_ERR_SSL_WANT_READ && n != MBEDTLS_ERR_SSL_WANT_WRITE)
                    return -1;
                continue;
            }
            sent += n;
        }
        return 0;
    }
#endif

    while (sent < len) {
        ssize_t n = send(client->sockfd, p + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

int oci_http_recv(OciHttpClient *client, void *buf, size_t buf_size, size_t *out_recv) {
    char *p = (char *)buf;
    size_t total = 0;

#ifdef MBEDTLS_SSL_H
    if (client->use_tls && client->tls_initialized) {
        while (total < buf_size - 1) {
            int n = mbedtls_ssl_read(&client->ssl, (unsigned char *)(p + total), buf_size - 1 - total);
            if (n <= 0) {
                if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE)
                    continue;
                if (n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
                    break;
                return -1;
            }
            total += n;
            p[total] = '\0';
            if (strstr(buf, "\r\n\r\n")) {
                const char *cl = strstr(buf, "Content-Length:");
                if (cl) {
                    cl += 15;
                    while (*cl == ' ') cl++;
                    int expected = atoi(cl);
                    const char *body_start = strstr(buf, "\r\n\r\n");
                    if (body_start) {
                        int body_len = total - (int)(body_start - (const char *)buf + 4);
                        if (body_len >= expected) break;
                    }
                }
            }
        }
        p[total] = '\0';
        if (out_recv) *out_recv = total;
        return total > 0 ? 0 : -1;
    }
#endif

    while (total < buf_size - 1) {
        ssize_t n = recv(client->sockfd, p + total, buf_size - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        p[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) {
            const char *cl = strstr(buf, "Content-Length:");
            if (cl) {
                cl += 15;
                while (*cl == ' ') cl++;
                int expected = atoi(cl);
                const char *body_start = strstr(buf, "\r\n\r\n");
                if (body_start) {
                    int body_len = total - (int)(body_start - (const char *)buf + 4);
                    if (body_len >= expected) break;
                }
            }
        }
    }
    p[total] = '\0';
    if (out_recv) *out_recv = total;
    return total > 0 ? 0 : -1;
}

int oci_http_request(OciHttpClient *client, const char *method, const char *path,
                     const char *extra_headers, const void *body, size_t body_len,
                     char *response, size_t resp_size) {
    char request[OCI_HTTP_BUF_SIZE];
    int n = snprintf(request, sizeof(request),
                     "%s %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: WuBuOS-OCI/1.0\r\n"
                     "Accept: application/vnd.oci.image.manifest.v2+json, application/json\r\n"
                     "Connection: close\r\n",
                     method, path, client->host);
    if (n < 0 || n >= (int)sizeof(request)) return -1;

    if (extra_headers && extra_headers[0]) {
        int m = snprintf(request + n, sizeof(request) - n, "%s\r\n", extra_headers);
        if (m < 0 || n + m >= (int)sizeof(request)) return -1;
        n += m;
    }

    if (body && body_len > 0) {
        int m = snprintf(request + n, sizeof(request) - n, "Content-Length: %zu\r\n", body_len);
        if (m < 0 || n + m >= (int)sizeof(request)) return -1;
        n += m;
    }

    int m = snprintf(request + n, sizeof(request) - n, "\r\n");
    if (m < 0 || n + m >= (int)sizeof(request)) return -1;
    n += m;

    if (body && body_len > 0) {
        if ((size_t)n + body_len >= sizeof(request)) return -1;
        memcpy(request + n, body, body_len);
        n += body_len;
    }

    if (oci_http_send(client, request, n) < 0) return -1;

    size_t recv_len;
    if (oci_http_recv(client, response, resp_size, &recv_len) < 0) return -1;

    /* Check HTTP status code */
    const char *status = strstr(response, "HTTP/");
    if (status) {
        status += 9; /* Skip "HTTP/1.1 " */
        int code = atoi(status);
        if (code >= 400) return -1; /* HTTP error */
    }

    /* Find body start (after headers) */
    const char *body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        size_t body_len = recv_len - (body_start - response);
        memmove(response, body_start, body_len);
        response[body_len] = '\0';
    } else {
        response[0] = '\0';
    }

    return 0;
}

/* Parse WWW-Authenticate header for Bearer token */
void oci_parse_auth_header(const char *response_headers, char *token, size_t token_size,
                           char *realm, size_t realm_size, char *service, size_t service_size,
                           char *scope, size_t scope_size) {
    const char *auth = strstr(response_headers, "WWW-Authenticate:");
    if (!auth) return;
    auth = strstr(auth, "Bearer");
    if (!auth) return;

    const char *realm_start = strstr(auth, "realm=\"");
    if (realm_start) {
        realm_start += 7;
        const char *realm_end = strchr(realm_start, '"');
        if (realm_end && realm) {
            size_t len = realm_end - realm_start;
            if (len >= realm_size) len = realm_size - 1;
            memcpy(realm, realm_start, len);
            realm[len] = '\0';
        }
    }

    const char *service_start = strstr(auth, "service=\"");
    if (service_start) {
        service_start += 9;
        const char *service_end = strchr(service_start, '"');
        if (service_end && service) {
            size_t len = service_end - service_start;
            if (len >= service_size) len = service_size - 1;
            memcpy(service, service_start, len);
            service[len] = '\0';
        }
    }

    const char *scope_start = strstr(auth, "scope=\"");
    if (scope_start) {
        scope_start += 7;
        const char *scope_end = strchr(scope_start, '"');
        if (scope_end && scope) {
            size_t len = scope_end - scope_start;
            if (len >= scope_size) len = scope_size - 1;
            memcpy(scope, scope_start, len);
            scope[len] = '\0';
        }
    }
}

/* Simple base64 encoding for Basic Auth */
void oci_base64_encode(const char *input, char *output, size_t out_size) {
    static const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t len = strlen(input);
    size_t i, j = 0;
    for (i = 0; i + 2 < len && j + 4 < out_size; i += 3) {
        uint32_t n = (input[i] << 16) | (input[i+1] << 8) | input[i+2];
        output[j++] = b64[(n >> 18) & 0x3F];
        output[j++] = b64[(n >> 12) & 0x3F];
        output[j++] = b64[(n >> 6) & 0x3F];
        output[j++] = b64[n & 0x3F];
    }
    if (i < len && j + 4 < out_size) {
        uint32_t n = input[i] << 16;
        if (i + 1 < len) n |= input[i+1] << 8;
        output[j++] = b64[(n >> 18) & 0x3F];
        output[j++] = b64[(n >> 12) & 0x3F];
        output[j++] = (i + 1 < len) ? b64[(n >> 6) & 0x3F] : '=';
        output[j++] = '=';
    }
    output[j] = '\0';
}