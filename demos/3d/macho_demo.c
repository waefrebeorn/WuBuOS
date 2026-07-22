/*
 * macho_demo.c -- WuBuOS macOS (Mach-O) 3D demo.
 *
 * A self-contained OpenGL hello-triangle, compiled to a Mach-O binary
 * via Darling's xclang/xcrun toolchain. Darling serves the Mach-O's
 * OpenGL calls through its GL shim onto the host Vulkan/Mesa stack
 * (lavapipe on this box), so this exercises a REAL 3D pipeline
 * -- the same capability layer (VSL) that drives the ELF/PE legs.
 *
 * This is the "agnostic operator for all hardware using our capabilities"
 * proof: a Darwin binary runs as a first-class WubuCt container
 * (CT_MACHO) via wubu_exec_launch_macho() -> wubu_ct_start()
 * (fork + chroot + exec + cgroup/seccomp isolation), launched by
 * `darling <this.macho>`.
 *
 * Uses only the portable OpenGL 1.1 core API (no framework deps
 * beyond OpenGL/gl.h and GLUT for a window), so it builds against
 * Darling's basic-headers + host GL headers.
 */
#include <OpenGL/gl.h>
#include <GLUT/glut.h>
#include <stdlib.h>

static int g_frames = 0;

static void render(void) {
    glClearColor(0.05f, 0.05f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_TRIANGLES);
        glColor3f(0.9f, 0.2f, 0.3f);  glVertex2f(-0.8f, -0.7f);
        glColor3f(0.2f, 0.9f, 0.4f);  glVertex2f( 0.8f, -0.7f);
        glColor3f(0.3f, 0.5f, 0.95f);  glVertex2f( 0.0f,  0.85f);
    glEnd();

    glutSwapBuffers();
    g_frames++;
    if (g_frames % 30 == 0)
        fprintf(stderr, "[macho_demo] rendered %d frames via OpenGL\n", g_frames);
}

static void idle(void) { glutPostRedisplay(); }

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(640, 480);
    glutCreateWindow("WuBuOS Mach-O 3D (Darling/OpenGL)");
    glutDisplayFunc(render);
    glutIdleFunc(idle);
    fprintf(stderr, "[macho_demo] Mach-O OpenGL demo started (PID %d)\n", (int)getpid());
    glutMainLoop();
    return 0;
}
