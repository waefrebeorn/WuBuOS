/* oci_tls_test.c — opt-in TLS verification gate for oci_http_connect.
 *
 * Requires mbedTLS headers/libs (WUBU_MBEDTLS_ROOT) and a local self-signed
 * cert. Two connections to a local mbedTLS server:
 *   1. no trusted CA bundle -> connect MUST fail (fail closed)
 *   2. the self-signed cert IS the CA bundle -> connect MUST succeed
 * A VERIFY_NONE implementation passes (1) or skips the CA entirely; this
 * gate catches both the old no-verification build and a broken CA path.
 */
#include <mbedtls/ssl.h>
#include "runtime/oci/oci_internal.h"
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include <mbedtls/error.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <unistd.h>

static int serve_once(const char *cert, const char *key, int listen_fd) {
    mbedtls_ssl_context ssl; mbedtls_ssl_config conf;
    mbedtls_x509_crt crt; mbedtls_pk_context pkey;
    mbedtls_entropy_context entropy; mbedtls_ctr_drbg_context ctr;
    mbedtls_ssl_init(&ssl); mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&crt); mbedtls_pk_init(&pkey);
    mbedtls_entropy_init(&entropy); mbedtls_ctr_drbg_init(&ctr);
    if (mbedtls_x509_crt_parse_file(&crt, cert) != 0 ||
        mbedtls_pk_parse_keyfile(&pkey, key, NULL) != 0) return 1;
    if (mbedtls_ctr_drbg_seed(&ctr, mbedtls_entropy_func, &entropy, NULL, 0) != 0) return 1;
    if (mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) return 1;
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr);
    mbedtls_ssl_conf_own_cert(&conf, &crt, &pkey);
    mbedtls_ssl_setup(&ssl, &conf);
    int fd = accept(listen_fd, NULL, NULL);
    if (fd < 0) return 1;
    mbedtls_ssl_set_bio(&ssl, &fd, mbedtls_net_send, mbedtls_net_recv, NULL);
    int rc = 0;
    while ((rc = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) break;
    }
    mbedtls_ssl_close_notify(&ssl);
    close(fd);
    mbedtls_ssl_free(&ssl); mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&crt); mbedtls_pk_free(&pkey);
    mbedtls_ctr_drbg_free(&ctr); mbedtls_entropy_free(&entropy);
    return rc == 0 ? 0 : 1;
}

static int try_connect(int port, const char *ca) {
    OciHttpClient c;
    memset(&c, 0, sizeof c);
    setenv("WUBU_CA_BUNDLE", ca, 1);
    int r = oci_http_connect(&c, "localhost", port, true);
    if (r == 0) oci_http_close(&c);
    return r;
}

int main(int argc, char **argv) {
    if (argc != 3) return 90;
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (bind(lfd, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(lfd, 8) != 0) return 91;
    socklen_t al = sizeof addr;
    getsockname(lfd, (struct sockaddr *)&addr, &al);
    int port = ntohs(addr.sin_port);

    pid_t p1 = fork();
    if (p1 == 0) _exit(serve_once(argv[1], argv[2], lfd));
    int r1 = try_connect(port, "/nonexistent-ca-bundle.pem");
    int st1; waitpid(p1, &st1, 0);

    pid_t p2 = fork();
    if (p2 == 0) _exit(serve_once(argv[1], argv[2], lfd));
    int r2 = try_connect(port, argv[1]);
    int st2; waitpid(p2, &st2, 0);
    close(lfd);

    printf("no-CA connect=%d (want <0), trusted-CA connect=%d (want 0)\n", r1, r2);
    if (r1 >= 0) { fprintf(stderr, "FAIL: accepted an unauthenticated server\n"); return 1; }
    if (r2 != 0) { fprintf(stderr, "FAIL: rejected a trusted server\n"); return 2; }
    printf("TLS VERIFY GATE PASS\n");
    return 0;
}
