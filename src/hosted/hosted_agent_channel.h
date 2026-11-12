/*
 * hosted_agent_channel.h -- the agent -> desktop channel.
 *
 * The MCP server runs on the host, not inside the OS, so it cannot call the
 * WM directly. It writes its requests into the agent's own namespace
 * (~/.wubu/n/agent/) and the desktop acts on them once per poll:
 *
 *   buddy_state        the mood name to show (work, error, success, ...)
 *   open_walkthrough   a flag file; the walkthrough opens and the flag clears
 *
 * The namespace is the federation's agent surface, so this is the same
 * channel an agent would use on any WuBuOS box.
 *
 * C11, opaque API, no god headers.
 */

#ifndef HOSTED_AGENT_CHANNEL_H
#define HOSTED_AGENT_CHANNEL_H

/* Look at the agent channel and act on anything new. Cheap: two file reads.
 * Call from the frame loop, throttled. */
void hosted_agent_channel_poll(void);

#endif /* HOSTED_AGENT_CHANNEL_H */
