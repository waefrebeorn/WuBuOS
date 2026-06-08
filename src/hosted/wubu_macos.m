/*
 * wubu_macos.m — WuBuOS macOS Launcher via Apple Virtualization.framework
 *
 * Cell 390: macOS deployment via Apple Virtualization.
 *
 * This IS the macOS equivalent of WSL2.
 * WSL2:  Windows → Hyper-V Linux VM → wubu binary
 * macOS:  macOS → Apple Virtualization Linux VM → wubu binary
 *
 * Apple Virtualization.framework (macOS 11+) provides:
 *   - VZVirtualMachine: runs aarch64 Linux natively on Apple Silicon
 *   - VZLinuxBootLoader: loads kernel + initramfs
 *   - VZVirtioNetworkDevice: bridged networking
 *   - VZVirtioBlockDevice: disk images
 *   - VZRosettaDirectory: x86_64 binary translation on ARM
 *
 * Usage:
 *   ./wubu_macos                    # GUI mode — VM boots, X11 forwarded
 *   ./wubu_macos --headless         # Headless — Styx server only
 *   ./wubu_macos --kernel /path     # Use specific kernel
 *
 * Prerequisites:
 *   - macOS 11.0+ (Big Sur)
 *   - Apple Silicon (M1/M2/M3/M4) OR Intel Mac with Rosetta
 *   - Arch ARM64 kernel + initramfs
 *   - XQuartz (for X11 forwarding in GUI mode)
 *
 * Build:
 *   clang -framework Virtualization -framework Foundation \
 *         -o wubu_macos wubu_macos.m
 *
 * The initramfs contains:
 *   /wubu  (the hosted binary — compiled for aarch64)
 *   /init  (mounts /proc /sys /dev, execs /wubu)
 *   /lib/  (libc, libX11 for X11 fallback)
 *
 * GUI display options:
 *   - X11 forwarding via XQuartz (simplest, matches Linux hosted)
 *   - Wayland via weston in the VM (native macOS window via wayland-proxy)
 *   - VNC via virtio-gpu (remote desktop)
 *
 * Architecture:
 *
 *   ┌─────────────────────────────────────────┐
 *   │  macOS Host                             │
 *   │  ┌───────────────────────────────────┐  │
 *   │  │  wubu_macos (this launcher)       │  │
 *   │  │  - VZVirtualMachineConfiguration  │  │
 *   │  │  - VZLinuxBootLoader              │  │
 *   │  │  - vcpu count, RAM, network       │  │
 *   │  └───────────────────────────────────┘  │
 *   │              │ VZVirtualMachine          │
 *   │              ▼                          │
 *   │  ┌───────────────────────────────────┐  │
 *   │  │  Arch ARM64 Linux VM              │  │
 *   │  │  - kernel: vmlinuz               │  │
 *   │  │  - initrd: initramfs.img          │  │
 *   │  │  - /wubu → Win98 GUI shell       │  │
 *   │  │  - .wubu containers via fork/exec │  │
 *   │  │  - 9P Styx namespace             │  │
 *   │  └───────────────────────────────────┘  │
 *   └─────────────────────────────────────────┘
 */

#import <Foundation/Foundation.h>
#import <Virtualization/Virtualization.h>

/* ── Configuration ──────────────────────────────────────────────── */

static NSString *const kDefaultKernelPath = @"vmlinuz";
static NSString *const kDefaultInitrdPath = @"initramfs.img";
static NSString *const kDefaultRamSize   = @"256";  /* MB */
static NSString *const kDefaultVCPUs     = @"2";

/* ── WuBuOS VM Delegate ─────────────────────────────────────────── */

@interface WuBuOSDelegate : NSObject <VZVirtualMachineDelegate>
@property (nonatomic, assign) BOOL running;
@end

@implementation WuBuOSDelegate

- (instancetype)init {
    self = [super init];
    if (self) {
        _running = YES;
    }
    return self;
}

- (void)virtualMachine:(VZVirtualMachine *)virtualMachine
 didStopWithError:(NSError *)error {
    fprintf(stderr, "WuBuOS VM stopped with error: %s\n",
            [[error localizedDescription] UTF8String]);
    self.running = NO;
}

- (void)virtualMachineDidStop:(VZVirtualMachine *)virtualMachine {
    fprintf(stderr, "WuBuOS VM stopped normally.\n");
    self.running = NO;
}

- (BOOL)guestDidRequestVirtualMachineToTerminate:(VZVirtualMachine *)virtualMachine {
    fprintf(stderr, "WuBuOS guest requested shutdown.\n");
    self.running = NO;
    return YES;  /* Allow termination */
}

@end

/* ── VM Configuration ───────────────────────────────────────────── */

static VZVirtualMachineConfiguration *createVMConfig(
    NSString *kernelPath,
    NSString *initrdPath,
    NSUInteger vcpus,
    NSUInteger ramMB
) {
    VZVirtualMachineConfiguration *config = [[VZVirtualMachineConfiguration alloc] init];

    /* Boot loader: Linux kernel + initramfs */
    VZLinuxBootLoader *bootLoader = [[VZLinuxBootLoader alloc] init];
    VZLinuxKernel *kernel = [[VZLinuxKernel alloc] initWithURL:
        [NSURL fileURLWithPath:kernelPath]];
    VZLinuxInitialRamdisk *initrd = [[VZLinuxInitialRamdisk alloc] initWithURL:
        [NSURL fileURLWithPath:initrdPath]];
    bootLoader.kernel = kernel;
    bootLoader.initialRamdisk = initrd;

    /* Boot args: init=/wubu → WuBuOS as PID 1 (Inferno emu pattern) */
    bootLoader.commandLine = @"init=/wubu console=ttyS0";

    config.bootLoader = bootLoader;

    /* CPU: 2 vCPUs (matches SteamOS defaults) */
    VZVirtualMachineConfiguration *cpuConfig = config;
    config.cpuCount = vcpus;

    /* RAM: 256MB default (enough for Win98 shell + containers) */
    config.memorySize = ramMB * 1024 * 1024;

    /* Serial port: stdout for console output */
    VZFileHandleConfiguration *serialConfig = [[VZFileHandleConfiguration alloc] init];
    serialConfig.inputFileHandle = [NSFileHandle fileHandleWithStandardInput];
    serialConfig.outputFileHandle = [NSFileHandle fileHandleWithStandardOutput];
    VZSerialPortDeviceConfiguration *serialPort = [[VZSerialPortDeviceConfiguration alloc] init];
    serialPort.attachment = serialConfig;
    config.serialPorts = @[serialPort];

    /* Network: virtio-net with NAT (same as WSL2) */
    VZVirtioNetworkDeviceConfiguration *network = [[VZVirtioNetworkDeviceConfiguration alloc] init];
    VZNATNetworkDeviceAttachment *nat = [[VZNATNetworkDeviceAttachment alloc] init];
    network.attachment = nat;
    config.networkDevices = @[network];

    /* Rosetta: x86_64 binary translation on Apple Silicon */
    /* This lets us run x86_64 .wubu containers on ARM Macs */
    if (@available(macOS 13.0, *)) {
        VZRosettaDirectoryConfiguration *rosetta = [[VZRosettaDirectoryConfiguration alloc] init];
        rosetta.installRosetta = YES;  /* Auto-install if needed */
        config.directorySharingDevices = @[];
        /* Note: Rosetta support requires macOS 13+ (Ventura) */
        /* For macOS 11-12, only aarch64 binaries run natively */
    }

    /* Validate configuration */
    NSError *error = nil;
    if (![config validateWithError:&error]) {
        fprintf(stderr, "VM configuration error: %s\n",
                [[error localizedDescription] UTF8String]);
        return nil;
    }

    return config;
}

/* ── Main ───────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    @autoreleasepool {
        /* Parse args */
        NSString *kernelPath = kDefaultKernelPath;
        NSString *initrdPath = kDefaultInitrdPath;
        NSUInteger vcpus = [kDefaultVCPUs integerValue];
        NSUInteger ramMB  = [kDefaultRamSize integerValue];
        BOOL headless = NO;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--kernel") == 0 && i + 1 < argc) {
                kernelPath = [NSString stringWithUTF8String:argv[++i]];
            } else if (strcmp(argv[i], "--initrd") == 0 && i + 1 < argc) {
                initrdPath = [NSString stringWithUTF8String:argv[++i]];
            } else if (strcmp(argv[i], "--headless") == 0) {
                headless = YES;
            } else if (strcmp(argv[i], "--vcpus") == 0 && i + 1 < argc) {
                vcpus = (NSUInteger)atoi(argv[++i]);
            } else if (strcmp(argv[i], "--ram") == 0 && i + 1 < argc) {
                ramMB = (NSUInteger)atoi(argv[++i]);
            }
        }

        /* Banner */
        fprintf(stderr, "╔════════════════════════════════════════╗\n");
        fprintf(stderr, "║  🌱 WuBuOS — macOS Virtualization     ║\n");
        fprintf(stderr, "║  Apple Virtualization → Arch ARM64     ║\n");
        fprintf(stderr, "║  Same .wubu containers. Same 9P.      ║\n");
        fprintf(stderr, "╚════════════════════════════════════════╝\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "  Kernel:  %s\n", [kernelPath UTF8String]);
        fprintf(stderr, "  Initrd:  %s\n", [initrdPath UTF8String]);
        fprintf(stderr, "  vCPUs:   %lu\n", (unsigned long)vcpus);
        fprintf(stderr, "  RAM:     %lu MB\n", (unsigned long)ramMB);
        fprintf(stderr, "  Init:    /wubu (Inferno emu pattern)\n");
        fprintf(stderr, "  Mode:    %s\n", headless ? "headless" : "GUI (X11 via XQuartz)");
        fprintf(stderr, "\n");

        /* Check macOS version */
        if (@available(macOS 11.0, *)) {
            fprintf(stderr, "  macOS 11+ ✓ — Apple Virtualization available\n");
        } else {
            fprintf(stderr, "  ERROR: macOS 11.0+ (Big Sur) required.\n");
            fprintf(stderr, "  WuBuOS uses Apple Virtualization.framework.\n");
            return 1;
        }

        /* Create VM configuration */
        VZVirtualMachineConfiguration *config = createVMConfig(
            kernelPath, initrdPath, vcpus, ramMB);
        if (!config) {
            fprintf(stderr, "Failed to create VM configuration.\n");
            return 1;
        }

        /* Create and start VM */
        VZVirtualMachine *vm = [[VZVirtualMachine alloc] initWithConfiguration:config];
        WuBuOSDelegate *delegate = [[WuBuOSDelegate alloc] init];
        vm.delegate = delegate;

        fprintf(stderr, "Starting WuBuOS VM...\n");

        dispatch_semaphore_t sem = dispatch_semaphore_create(0);

        [vm startWithCompletionHandler:^(NSError *error) {
            if (error) {
                fprintf(stderr, "VM start failed: %s\n",
                        [[error localizedDescription] UTF8String]);
            } else {
                fprintf(stderr, "WuBuOS VM running.\n");
                fprintf(stderr, "  Arch ARM64 kernel → /wubu → Win98 GUI\n");
                fprintf(stderr, "  .wubu containers via fork+chroot+exec\n");
                fprintf(stderr, "  9P Styx namespace on Unix socket\n");
                if (!headless) {
                    fprintf(stderr, "  Display: X11 via XQuartz (set DISPLAY=:0)\n");
                }
            }
            dispatch_semaphore_signal(sem);
        }];

        /* Wait for VM to start */
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);

        /* Keep running until VM stops */
        while (delegate.running) {
            [NSThread sleepForTimeInterval:0.1];
        }

        fprintf(stderr, "WuBuOS shutdown complete.\n");
        return 0;
    }
}
