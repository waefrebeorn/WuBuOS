# The only Mac I have is a 2012 iMac

The only Mac I own is a 2012 iMac. It still boots and it still runs macOS. People hear "2012" and tell me I need to upgrade before I can develop for macOS. I don't. And I'm not going into debt to find out.

Let me be plain about the money. A payment plan on a Mac is debt. It doesn't matter that Apple makes the monthly number look small, or that the machine is "for work." The work isn't paying me yet. WuBuOS is a project I'm building, not a salary I'm drawing, and financing hardware against income that doesn't exist is how a project turns into an obligation. I refuse. Not because I can't do the arithmetic, because I can. A new Mac is a couple thousand dollars of debt to compile code that a free CI runner will compile for me while I sleep.

What the project actually needs is not a Mac on my desk. It's a macOS verification loop. WuBuOS is a WSL replacement: develop in Linux, run in macOS. The desktop guest is a plain Wayland client. The compositor is the seam that touches the host OS, X11 today, Quartz tomorrow. None of that requires me to own new Apple hardware. It requires macOS somewhere, running my code, reporting back, with no human in the loop.

That last part is the whole thing. I don't have time for telephone tag. The usual advice, rent a Mac in the cloud, ask some macOS guy, join a Discord, file a ticket, assumes I have hours to spend being someone's support case. I don't. Rent-a-Mac services, the RunPod model applied to Apple hardware, are fine when an agent can drive them end to end over an API; NoMac sells exactly that, a root session an agent can take over SSH or MCP. But paid is still paid, and it still needs a card and a balance. If the machine needs me to chat with a nerd, click through a portal, or wait on a person, it isn't a tool. It's a second job. Automation or nothing.

The research in this folder found that the loop already exists and it's free. GitHub runs macOS runners, Intel and Apple Silicon, free and unlimited on public repositories. My repositories are public. Push a branch and a real Mac compiles it, runs the gates, posts the result; the agent reads the log and iterates. Nothing to buy and nobody to call. Cirrus CI, which half the old blog posts still recommend, stopped running jobs in June 2026. The research notes the dead ends so nobody wastes a week on one. MacStadium gives free Mac minis to open-source projects, but there's an application and a waitlist, which is a human gate, so it's a backup and not the plan.

And the 2012 iMac is not useless. It's Apple hardware, which means it's legally allowed to run macOS virtual machines. OpenCore Legacy Patcher pushes it past Catalina, and it can be an SSH target or a self-hosted runner for the cases CI can't reach. It's the fallback, not the foundation. The foundation is the free loop.

There's a principle under all of this. WuBuOS runs where people are, on the hardware they already have. If building it required buying a new Mac on credit, the philosophy would be a lie before it shipped. Debt is not a build dependency. The 2012 iMac and a free CI loop are enough, and I'd rather prove that than finance the alternative.

## Addendum: it was never just about the Mac

Linux and Windows get the same deal. People on those hosts run WuBuOS two ways: hosted, as a Wayland guest under our own compositor (natively on Linux, through WSL2 and WSLg on Windows, which is the WSL-replacement shape exactly), or booted, the GRUB ISO in QEMU, hooked to the host over serial and 9P. One image boots on all three host operating systems, and the CI now proves it: build once on Linux, boot under QEMU on a Linux runner, a Windows runner, and a Mac runner, assert the console comes up on each. Free, on every push.

We ship source as the distro. The repo is the distribution: clone, make, run, or take the ISO and boot it. The build recipes are the product and the verify loops are the quality department, and neither costs money or waits on a person.

The kernel design is TempleOS's, expanded. Ring-0, one address space, the whole machine as one organism you can hold in your head. WuBuKernel carries that forward in our own C11, and WuBuOS is the body it grows into. Nobody should need a new computer or a payment plan to run this. An OS that boots everywhere is an OS that belongs to everyone, and that is the point.