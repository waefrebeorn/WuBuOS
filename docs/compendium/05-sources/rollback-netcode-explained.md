---
source_url: https://arstechnica.com/gaming/2019/10/explaining-how-fighting-games-use-delay-based-and-rollback-netcode/
title: Explaining how fighting games use delay-based and rollback netcode - Ars Technica
ingested: 2026-08-03
avenue: Games
---

# Explaining how fighting games use delay-based and rollback netcode - Ars Technica

[Skip to content](https://arstechnica.com/gaming/2019/10/explaining-how-fighting-games-use-delay-based-and-rollback-netcode/#main)

Text
settings

Story text

SizeSmallStandardLargeWidth
\*StandardWideLinksStandardOrange

\\* Subscribers only

[Learn more](https://arstechnica.com/store/product/subscriptions/)

Minimize to nav


Ricky "Infil" Pusch is a long-time fighting game fan and content creator. He wrote [The Complete Killer Instinct Guide](http://ki.infil.net/), an interactive and comprehensive website for learning about Killer Instinct. This article was originally [published there](http://ki.infil.net/w02-netcode.html).

Hang around the fighting game community for any period of time, and you’ll hear discussion about why playing fighting games online can be frustrating. A genre built on twitch reflexes and player reactions, fighting games can struggle at times to translate their offline success to online environments. Good online play is possible, though, and nothing is more important for realizing this goal than choosing the right approach to netcode.

At its core, netcode is simply a method for two or more computers, each trying to play the same game, to talk to each other over the Internet. While local play always ensures that all player inputs arrive and are processed at the same time, networks are constantly unstable in ways the game cannot control or predict. Information sent to your opponent may be delayed, arrive out of order, or become lost entirely depending on dozens of factors, including the physical distance to your opponent, whether you’re on a Wi-Fi connection, and whether your roommate is watching Netflix.

Online play in games is nothing new, but fighting games have their own set of unique challenges. They tend to involve direct connections to other players, unlike many other popular game genres, and low, consistent latency is extremely important because muscle memory and reactions are at the core of virtually every fighting game. As a result, two prominent strategies have emerged for playing fighting games online: delay-based netcode and rollback netcode.

There’s been a renewed sense in the fighting game community that rollback is the best choice, and fighting game developers who [choose to use delay-based netcode](https://www.youtube.com/watch?v=qW61xJNJ9m8) are [preventing the growth of the genre](https://www.youtube.com/watch?v=iTUtnclr2hs). While people have been passionate about this topic [for many years](https://www.youtube.com/watch?v=Tu2kAdmUCaI&t=42m34s), frustrations continue to rise as new, otherwise excellent games repeatedly have bad online experiences.

There are relatively few easy-to-follow explanations for what exactly rollback netcode is, how it works, and why it is so good at hiding the effects of bad connections (though [there are some](http://mauve.mizuumi.net/2012/07/05/understanding-fighting-game-networking.html)). Because I feel this topic is extremely important for the future health of the fighting game community, I want to help squash some misconceptions about netcode and explain both netcode strategies thoroughly so everyone can be informed as they discuss. If you stick around to the end, I’ll even interview some industry experts and community leaders on the topic!

Before we dig into the details, though, let’s get one thing straight.

## Why should I care?

Both companies and players should care about good netcode because playing online is no longer the future—it’s the present.

While most other video game genres have been this way for a decade or longer, fighting game developers seem to be resistant to embracing online play, perhaps because of the genre’s roots in offline settings such as arcades and tournaments. Playing offline is great, and it will always have considerable value in fighting games, but it’s simply the reality that a large percentage of the player base will never play offline. For many fighting game fans, playing online is the game, and a bad online experience prevents them from getting better, playing or recommending the game to their friends, and ultimately causes them to [simply go do something else](https://youtu.be/iTUtnclr2hs?t=758).

Even if you think you have a good connection, or live in an area of the world with robust Internet infrastructure, good netcode is still mandatory. Plus, lost or delayed information happens regularly even on the best networks, and poor netcode can actively hamper matches no matter how smooth the conditions may be. Good netcode also has the benefit of connecting regions across greater distances, effectively uniting the global player base as much as possible.

Bad netcode can ruin matches. This match, played online between two Japanese players, impacted who gets to attend the Capcom Pro Tour finals. ( [source](https://twitter.com/john_takeuchi/status/1162562266027327488))

Bad netcode can ruin matches. This match, played online between two Japanese players, impacted who gets to attend the Capcom Pro Tour finals. ( [source](https://twitter.com/john_takeuchi/status/1162562266027327488))



What about those who never play online because they prefer playing offline with their friends? The healthy ecosystem that good netcode creates around a game benefits everyone. There will be more active players, more chances to consume content for your favorite game—from tech videos to spectating online tournaments to expanding the strategy of lesser-used characters—and more excitement surrounding your game in the fighting game community (FGC). Despite _[Killer Instinct](http://ki.infil.net/)_’s pedigree as an excellent game, there’s no doubt that its superb rollback netcode has played a huge part in the sustained growth of its community.

Good netcode matters, period. So let’s talk about it.

## The basics

Before we get into the specifics of how the two netcode strategies work, we first have to set a few ground rules that govern fighting games and introduce a few terms.

In fighting games, time is measured in a unit called a frame. Just to make the discussion easier, we’ll assume that all fighting games operate at 60 frames per second, which means that one frame is around 16 milliseconds (ms) of real time.

Importantly, this isn’t just how fast the game renders new images to your screen; every frame, the game executes its game loop, which (among other things) asks the players’ controllers for inputs, checks the network for new information, runs AI for any CPU players, animates the moves each character is doing, and checks if someone is now getting hit. After it has done all these things, it draws the results of all its calculations to the screen, then does it all again 16 milliseconds later. In fighting games, this loop needs to be tight and consistent for all players who play your game, regardless of how fast or slow their computer is.

When playing a fighting game offline against your friend, you connect two controllers to one computer or console. If you both happen to press a button within the same 16 millisecond window, the game will receive and process the inputs on the same frame and apply the logic as expected. You will both see the same output, because there is only one computer doing the calculations.

The inputs for each player are shown at the bottom. When playing offline, there is no trouble processing all inputs for both players as soon as they are pressed.

The inputs for each player are shown at the bottom. When playing offline, there is no trouble processing all inputs for both players as soon as they are pressed.



The inputs for each player are shown at the bottom. When playing offline, there is no trouble processing all inputs for both players as soon as they are pressed.

This changes when two players are playing over the Internet.

First of all, information always takes time to send through a network. This is measured in ping, the amount of time it takes for information to be sent to the other player and then back to you. Over a connection with 90ms ping, for example, it takes 45ms (on average) for information to reach the other side, which is about three in-game frames. This means games now need to be clever about how they handle the input part of their game loop, as they can no longer guarantee button presses for the remote player will line up with the local player.

When playing online, your own inputs are still processed immediately, but the remote player’s inputs now take time to travel over the network. The game needs to decide how to handle this so both games remain in sync.

When playing online, your own inputs are still processed immediately, but the remote player’s inputs now take time to travel over the network. The game needs to decide how to handle this so both games remain in sync.



Secondly, two different computers are now trying to run two copies of the game at the same time but still produce identical results for both players. That’s why it’s a great idea for fighting games to be deterministic—given identical inputs, every machine that runs the game must produce identical results. This is cool for non-networking features like replays because you can simply save the inputs from each player and always reconstruct the match perfectly, but it also means the game only needs to send player inputs over the network to play online matches. We get to avoid sending complicated information about the game state and can save a lot of bandwidth.

When games send information to each other and then rely on the computers to independently run the simulations in sync, it’s said they are using lockstep networking. They don’t talk to a central authority that keeps track of the game for them and tells them what to do, like a server. Instead, they police themselves by asking each other periodically if they have the same game state. If the games start to disagree about the state of the game, they are desynced and will probably just have to abandon the match entirely. Games talking to each other directly can often be faster than being forced to talk through an intermediary, and lockstep solutions are particularly good at preventing many types of cheating. For example, even if you hacked your game so Ryu can throw faster fireballs, my simulation of the game won’t agree with yours and we will quickly desync.

Now, after all this setup, we can finally get to the meat of the problem. If we have a deterministic fighting game that uses lockstep networking, the only thing we need to play online matches is the input from both players. Let’s talk about these “clever ways” a fighting game handles player inputs that are never received on time.

## Delay-based netcode

As a reminder of the problem, fighting games are used to process inputs from both players at the same time. When playing online, your own inputs are received immediately, but your opponent’s inputs for the same frame need to be sent over the network and will arrive late. The game needs a strategy for how it will deal with these late inputs while keeping the game feeling as close to offline play as possible.

(As a side note, we won’t even talk about the naive solution where both clients simply wait for remote input before completing the game loop for each frame. Here, you could wait 100ms or more, instead of the tight 16ms we require. This would [slow the simulation down to a crawl](https://medium.com/@meseta/netcode-concepts-part-3-lockstep-and-rollback-f70e9297271) and make your fighting game instantly unplayable.)

The first strategy, called delay-based or input delay, is the simplest and most common implementation for games using lockstep networking. If a remote player’s inputs are arriving late because they need to be sent over the network, delay-based strategies simply artificially delay the local player’s inputs by the same amount of time. Then, in theory, both inputs will “arrive” at the same time and can be executed on the same frame as expected.

Let’s illustrate this more clearly with a video example. Here, two players are playing UNIST, a fighting game that uses delay-based netcode, over a connection that has 90ms ping. This means it takes half of that time on average (45 ms, or about three in-game frames at 16ms each) for your opponent’s input to reach you. The local player presses a button on frame three. In offline play, we would see this move begin to animate immediately, but instead, the game will delay the input by three frames and begin executing the move on frame six.

In delay-based netcode, when the local player presses a button, it is artificially delayed (here, by 3 frames).

In delay-based netcode, when the local player presses a button, it is artificially delayed (here, by 3 frames).



These extra three frames give the necessary time for your opponent’s input from frame three to reach you over the Internet. On frame six, we’ll have input from both players and the game can proceed. As long as every input from your opponent can travel the network in these three frames, the game remains stable and consistent.

Both players press a button on the same frame. The artificial delay for the local player gives time for the remote player’s input to reach them, so they can still be executed on the same frame.

Both players press a button on the same frame. The artificial delay for the local player gives time for the remote player’s input to reach them, so they can still be executed on the same frame.



Unfortunately, this is not the reality of the Internet.

## The problems with delay-based netcode

You might suspect the main problem is that, if your inputs are delayed any amount, it will hugely impact your reactions and game plan and make the game unplayably different from offline matches. While delays are not ideal, there are many players who will not be able to notice small delays on the order of two or three frames. For those who can notice it, or are training to do well at offline tournaments where there will be no delay, clever game design and the delay remaining consistent can mitigate the effects of the delay surprisingly well and still allow online play to be a useful method of practice.

But no, the main problem is consistency. Because networks are notoriously inconsistent, delay-based strategies struggle because they are terrible at handling network fluctuations. Suppose there is a spike in the connection and some information from your opponent takes a little longer than the estimated three frames to reach you. Because the game simply cannot proceed without information from both players, it has no choice but to stop and wait for the input to arrive, causing the game to [chug and lurch along](https://clips.twitch.tv/LightSassyCrabsRedCoat) during moments of prolonged network trouble.

At 30% speed, you can see the game having to pause during network trouble. A frame-by-frame view shows the remote player’s input getting lost for 6 frames, and the game has no choice but to sit there and wait for the input to arrive before it can continue.

At 30% speed, you can see the game having to pause during network trouble. A frame-by-frame view shows the remote player’s input getting lost for 6 frames, and the game has no choice but to sit there and wait for the input to arrive before it can continue.



Since pausing the game even briefly is pretty detrimental to a player’s experience, most delay-based implementations will try to prevent it from happening as much as they can. By keeping an eye on the network conditions, they can change the amount of input delay on the fly to match the current connection health. But because network behavior is very difficult to predict, it will often change too late to avoid having a few game pauses, and it will often keep the input delay inflated for longer than is needed. For example, before [switching](https://www.polygon.com/2016/1/15/10776282/mortal-kombat-x-network-code-ggpo) to a rollback approach, _Mortal Kombat X_’s delay-based netcode would fluctuate between [five and 20 frames of delay](https://youtu.be/7jb0FOcImdg?t=703), and connection spikes would inflate the delay for many seconds, even if the spike only affected a few frames. It can [lead to instances](https://www.twitch.tv/videos/376451551?t=02h15m37s) where players feel the game is [lagging a lot more](https://youtu.be/tft6LBp3lZI?t=1782) than it is reporting.

Guilty Gear Xrd changes the input delay on the fly to hopefully avoid needing to pause the game and wait for inputs. Sometimes the fluctuations are small, and sometimes they can get out of control.

Guilty Gear Xrd changes the input delay on the fly to hopefully avoid needing to pause the game and wait for inputs. Sometimes the fluctuations are small, and sometimes they can get out of control.



Delay-based netcode is also hyper-sensitive to the distance between you and your opponent, because a greater distance increases the travel time across the network and therefore the game must also increase the artificial input delay. Distance plays a factor in all online connections regardless of netcode choice, but it’s particularly difficult to play delay-based games even just a single time zone away, let alone trying to unite the player base globally.

And finally, one last drawback that may not be obvious at first is that delay-based solutions do not care about the game state. Whether the announcer is calling “Round 2, Fight,” your opponent is knocked down, or both players are walking around in neutral, a delay-based game treats connection issues identically in all cases. This means there is no place to hide from a bad connection, even if your opponent’s inputs would not impact the match in any way.

Ultimately, delay-based solutions fail to provide a good user experience because, unlike offline play, player inputs are not consistent or responsive. Fighting game players thrive on reproducible and trainable situations that are tied heavily to reactions and reflexes. If the opponent jumps, I can input my anti-air. If I land this combo starter, I can continue the combo with muscle memory. When I knock my opponent down, I can [meaty](http://iplaywinner.com/glossary/general-terms/meaty.html) them with this specific setup. When input delay fluctuates wildly, whether it’s between two different matches or even during the same match, fighting game players lose confidence that anything they do will work as expected. And when they are unable to enact the game plan they know would help them win, online play becomes extremely frustrating and often useless.

## But what if I have a good connection?

People drastically overrate their connections. While it’s possible you may have a fast, stable connection to some people near you, it’s impossible to control the quality of the connections to every other potential player. Some players may be far away from you or might have a higher tendency to drop information over the network. Even two people near each other with normally fast connections may struggle to maintain a stable connection to each other, for reasons uncontrollable by either party.

But yes, it’s possible that, in some subset of connections with suitably small input delay, some matches in delay-based games will perform well. However, netcode needs to be judged [based on how it handles bad connections, not good ones](https://twitter.com/TheKeits/status/1053011895404367872?s=20). If given a perfect connection with inputs that always sync up, there is no need for any clever strategy to approximate offline play—you’re already there! It’s the netcode’s job to cleverly hide latency, and if your strategy falls apart at even the slightest sign of network trouble, then it’s not an effective method after all.

## Why is delay-based still used?

The main reason so many fighting games implement delay-based netcode is because it is relatively simple. Your gameplay logic is largely unchanged from offline play, since you can always assume both players’ inputs are provided at the start of your game loop. There is also no extra game state processing that needs to occur, which means your game takes roughly the same amount of resources from the computer to run both offline and online matches. It is likely the cheapest, easiest option for developers seeking to provide a barebones online mode.

Unfortunately, it has been mostly Japanese developers who continue to use delay-based approaches, as virtually every fighting game made in America has given up on delay-based netcode and made the switch to rollback. This includes titles from big publishers, like _Killer Instinct_ and _Mortal Kombat 11_, to smaller indie games like _Skullgirls_, _Punch Planet_, _Pocket Rumble_, and _Them’s Fightin’ Herds_. It’s possible the larger geographic area of America and Europe has encouraged these developers to make netcode a bigger priority, but it’s not like people playing in Japan are safe from [bad connections](https://youtu.be/QFXkFxe06DY?t=1273) and the [frustration of bad netcode](https://clips.twitch.tv/GloriousKnottyCardPeteZaroll), even though they live closer together and have strong online infrastructure.

Due to the insistence of fans and trends in the industry, it’s also increasingly unlikely that Japanese developers are simply unaware of rollback as an option for their games. Capcom, one of the premier Japanese fighting game developers, has attempted rollback netcode in three different titles ( _Street Fighter_ x _Tekken_, _Street Fighter V_, _Marvel vs. Capcom: Infinite_), with [varying degrees of success](https://twitter.com/yukimayucom/status/1169970222167724033). Daisuke Ishiwatari, chief creative officer at Arc System Works, admitted that fans have been [bugging him about rollback netcode, but they still chose to use in-house delay-based netcode](https://venturebeat.com/2018/03/05/how-arc-system-works-is-aiming-for-a-simpler-fighter-with-blazblue-cross-tag-battle/) for 2018’s _Blazblue: Cross Tag Battle_. Some [fans](https://youtu.be/qW61xJNJ9m8?t=12) (and even some [developers](https://youtu.be/Tu2kAdmUCaI?t=2594)) have theorized that this may be due to some companies’ preference to write their own solutions to solve problems, an issue that seems to [especially impact Japanese studios](https://www.eurogamer.net/articles/2012-10-10-tokyo-story-a-postcard-from-the-japanese-games-industry). But because so many popular fighting games in our community are made by Japanese developers, delay-based netcode continues to be prevalent. More than a decade after rollback netcode was first pioneered, only two of the nine games at EVO 2019 used rollback for their online multiplayer, and only one of these two games has had good fan reception to their netcode.

With all this said, it’s time to talk about exactly how rollback works.

## Rollback netcode

Since a game’s choice of netcode can never magically change the distance between a player and their opponent or prevent networks from dropping or delaying information, you may wonder how one netcode strategy could be drastically better than any other. The key lies in how the netcode handles uncertainty.

When there is no information from the remote player, delay-based netcode needs to pause and wait, as described in detail on the previous page. Rollback’s main strength is that it never waits for missing input from the opponent. Instead, rollback netcode continues to run the game normally. All inputs from the local player are processed immediately, as if it was offline. Then, when input from the remote player comes in a few frames later, rollback fixes its mistakes by correcting the past. It does this in such a clever way that the local player may not even notice a large percentage of network instability, and they can play through any remaining instances with confidence that their inputs are always handled consistently.

But let’s start by taking a look at what a “rollback” even is.

## Act now, apologize later

When remote inputs are missing, rollback netcode will continue to simulate the game. But, when those inputs eventually come in, the game will have advanced past the time when the opponent pressed that button, and the game will have already shown a different result to the screen. To fix this, rollback netcode will rewind the simulation, apply the correct input, and show the new result to the player immediately.

In the example below, both the local player and the remote player press medium punch on frame 1, but let’s say the opponent’s input is delayed over the network and reaches us three frames later on frame 4. Like offline play, the local player’s move immediately begins animating on their screen, and the game happily continues along assuming the opponent has not done anything. On frame 4, input from the opponent, marked as being pressed on frame 1, finally arrives. This means what we’ve shown to the player in the last 3 frames is not what really happened, and the game must fix the past by performing some calculations in the background.

First, the game state gets reloaded to what it was before frame 1—that is to say, it “rolls back” to the frame where the input needs to be applied, a few frames in the past. Then, the inputs from the remote player (as well as any inputs the local player had pressed during past frames) are applied, and the game re-simulates multiple frames forward to reach the present frame once again.

The remote Jago player presses MP on frame 1. When it reaches us, we’re already on frame 4, so we have to rewind to frame 1, get Jago to press MP, then re-simulate back to frame 4 very quickly. The orange filter and “ghosted” version of Jago indicate background calculations that the player never sees. At 30% speed, the local player just sees Jago’s MP appear a few frames into its startup.

The remote Jago player presses MP on frame 1. When it reaches us, we’re already on frame 4, so we have to rewind to frame 1, get Jago to press MP, then re-simulate back to frame 4 very quickly. The orange filter and “ghosted” version of Jago indicate background calculations that the player never sees. At 30% speed, the local player just sees Jago’s MP appear a few frames into its startup.



All of this happens instantly, in one game frame; the local player does not ever see the game perform these steps individually. Instead, all they see is the game state they thought was correct (but was false) get immediately replaced with the actually correct game state. Depending on what’s happening in the game, the characters might suddenly jump around a little bit, and in general, animations for the remote player’s moves will tend to appear a few frames already into their startup when they are shown to the local player.

Jago attacks with his overhead. The video shows the move with 0, 1, 2, or 3 frames of rollback right when Jago attacks, which shaves that same number of frames off the beginning of the move’s startup. Even at 30% speed, it’s hard to tell the difference.

Jago attacks with his overhead. The video shows the move with 0, 1, 2, or 3 frames of rollback right when Jago attacks, which shaves that same number of frames off the beginning of the move’s startup. Even at 30% speed, it’s hard to tell the difference.



To put it another way, rollback allows each client to temporarily break the lockstep model—the game may be showing slightly different things to each player, depending on the connection quality and what’s happening at the time of the rollbacks. However, the game will always correct itself to be the same for both players whenever the inputs have been received a few frames later.

It’s worth reiterating this important property of rollback netcode; the local player’s inputs are always shown immediately and can never be undone. If you press a button on frame four, that information is immediately processed by your game, and your move will instantly come out on your screen. If any rollback occurs when you pressed that button, it is always correctly re-applied during the rollback calculation. There is also no way your input can be invalidated or “eaten” by network lag, which can occur in delay-based frameworks when the game is waiting for remote input and is unresponsive for both players. Therefore, a player can feel confident that the buttons they press will be executed regardless of the network quality, greatly enhancing the consistency of online play.

As an added bonus, when a game encounters network trouble, there will only ever be rollbacks in the immediate time around the spike, making it a very localized approach. In delay-based solutions, the game may choose to inflate the input delay for many seconds, causing long-lasting effects far away from the time of trouble, even though the network may have smoothed out its issues by then. Rollback is particularly great for connections with high likelihood for packet loss, like Wi-Fi, for this reason.

This is the core concept of rollback. But we can do better.

## The basics of prediction

In the example above, when input was missing for the remote player, we simply assumed they were inputting nothing so that the game had something to simulate. This is a pretty bad assumption in general because players are rarely doing nothing, and the visual states shown to the player right before a rollback would almost always be wildly incorrect. Turns out, we can predict what the opponent is doing with an uncanny degree of accuracy.

When input is missing, what rollback solutions actually do is duplicate the last known input from the remote player for the current missing frame. If they were holding down-back, the game predicts they will continue to hold down-back. If they were holding a button, the game assumes they are probably still holding that button. The game then runs with these predicted inputs for the remote player instead.


[... middle omitted — see footer ...]


_How do you think the ecosystem around these games would change if they implemented rollback netcode? How would the players, commentators, and fans of the game benefit?_

**Sajam**: Well for starters, the amount of possible opponents for somebody to play with online expands by an incredible amount. That alone is a huge deal for communities with smaller player pools, or for regions who want to train together but can’t because of a large distance. We see this with SonicFox training consistently with players from all over the US, and Europe in _Mortal Kombat 11_ every week on their stream.

Besides casual and competitive players gaining possible opponents, it gives credibility to online events/tournaments. Events like the Capcom Pro Tour Online gain respect as a viable method for determining who should be handed out points on their competitive circuits. It also lets a developer run ranked competitions or leagues knowing there is validity to it. For commentators, all that online match footage from tournaments becomes usable, and we as commentators would have more access to online tournaments without the need to travel. I’ll stop here, because I think I have an endless list of reasons why it benefits tons of different types of fighting game fans and players.

_A lot of those benefits seem targeted towards competitive players or those who are super invested in the game. Do you think the casual player would notice rollback over a basic delay-based solution? You often hear comments like “when I play the game, there’s no lag” coming from casuals who might not even think twice if rollback was in the game. Should the devs only put rollback in because of the hardcore community, or are there benefits for the casual fans too?_

**Sajam**: Oh there are definitely benefits for casual fans. For starters, lots of casual fans who play games on console use Wi-Fi, and that is really brutal for delay based solutions. Rollback is much better at handling inconsistent connection types, and making it seem like there was no lag there at all. In the case of casual fans, expanding the amount of friends they are able to play with it based on distance, and making online play feel great is such a huge win. It’s hard to explain how often I hear people say that bad netcode, and lack of good online features really hurt somebody’s enjoyment of fighting games. As far as noticing delay based solutions vs rollback goes, a big reason rollback is great is actually how well it masks bad lag.

_A point of view that I’ve heard from some players is that when a game has good netcode, people would rather sit at home and play online than travel to locals or tournaments for a game. Do you think there’s any merit to this idea that good netcode may actually, in some ways, be a “bad thing” or have a negative impact on local scenes?_

**Sajam**: I’ve heard the same thing, and I feel like it’s a pretty interesting thing to consider. As good as netcode can be, it won’t replace the feeling of playing fighting games offline, and it can never replace the community aspect that exists in our grassroots events and tournaments. That being said, I can definitely see the ability to play lots of people online with a smooth connection be tempting over attending a local occasionally. In that case, I think it opens up the door to play with those some local friends online, or even host local/regional events weekly online. In either case, I don’t expect it steps on the toes of larger regionals or majors very much at all. Those are worth attending, regardless of how good online play is.

_Often when consumers are unhappy with something, they choose to “vote with their wallet”. In the case of fighting games, there’s been some sentiment that maybe the community at large should stop supporting games that don’t implement rollback so the developers get some tangible feedback. On the other hand, it’s very hard to not be excited about some fighting games on the horizon, even if it’s unlikely that they’ll use rollback. What would you say to someone who’s caught in the middle of wanting to play cool games, but also wanting to try and vote with their wallet about the rollback issue?_

**Sajam**: I mean that’s a tough spot to be, and I say that as somebody who is in that exact position all the time. I feel that exact way about the upcoming _Guilty Gear_, but then I have hope for stuff like the Riot fighting game announced recently as well. I think you can vote with your wallet if netcode is a deal breaker to you, and at the very least we should all demand that developers consider netcode when creating games in the modern era.

That being said, it can be tough to resist with how many great fighting games come from Japan with delay based netcode. Being a fighting game fan is full of these moments of, “Man this is so close to being awesome, but it’s too bad they didn’t do this or that.” And feeling that all the time gets pretty tough I think, especially when you look at other competitive games out there. Especially when many of the things we want in fighting games, like good netcode, have existed for over a decade now. It’s really a shame, I think.

_Do you think SFV’s implementation of rollback has done damage to rollback’s reputation as a good online solution for fighting games?_

**Sajam**: I think it undoubtedly has, yeah. Unfortunately, Capcom tried to do right by implementing rollback netcode, but had some issues, some of which were mentioned in this article, that lead to the current state of SFV’s online play. That being said, it’s important for articles like this, and creators like myself to use our voices and let people know that rollback is still the best option available.

Some people have even cited that they enjoy the netcode in SFV more than SF4, which is really fascinating to me. The idea that somebody would enjoy a rollback solution that doesn’t function correctly, versus a pretty standard delay based solution, is a good sign that they were on the right path. Still, if SFV’s version of rollback has soured you, I can’t blame you. I do encourage you to look at games like _Killer Instinct_, _Punch Planet_, _Skullgirls_, _Mortal Kombat 11_, etc that have done rollback correctly!

_Any closing thoughts you want to share with people who play lots of online fighting games?_

**Sajam**: Yeah! Mostly that adding rollback netcode is tough if developers don’t decide that’s the route for them from the start, so any time a developer uses it make sure to thank them and appreciate how awesome fighting games with good netcode are! Besides that, thank you for writing this awesome article, and thanks to everybody who took the time to read this interview!

I would like to thank [krazhier](https://twitter.com/Krazhier) and [Keits](https://twitter.com/TheKeits) for taking hours out of their busy schedules to discuss technical aspects of netcode with me, and [Sajam](https://twitter.com/Sajam) for taking time to answer interview questions and being supportive throughout the writing process. I would also like to especially thank [MagicMoste](https://twitter.com/MagicMoste) for making all the wonderful videos you see in this article. All their help was offered for free and I am thankful for their friendship.

Also thank you, the reader, for sticking with this article to the end! My sincere hope is that you learned something new about the challenges of making fighting games play well online, and if conversations come up about rollback netcode with your friends or other members of the FGC, I hope it’s easier to have informed discussions with them. Thanks for reading, and until next time!

Listing image:
Aurich Lawson / Capcom / Getty Images


[139 Comments](https://arstechnica.com/gaming/2019/10/explaining-how-fighting-games-use-delay-based-and-rollback-netcode/#comments "139 comments")

Comments


[Forum view](https://arstechnica.com/civis/threads/explaining-how-fighting-games-use-delay-based-and-rollback-netcode.1459983/)

![Loading](https://cdn.arstechnica.net/wp-content/themes/ars-v9/public/images/firework-loader.75ab30.gif)
Loading comments...


[Prev story](https://arstechnica.com/information-technology/2019/10/air-force-finally-retires-8-inch-floppies-from-missile-launch-control-system/ "Go to: Air Force finally retires 8-inch floppies from missile launch control system")

[Next story](https://arstechnica.com/science/2019/10/report-more-than-half-of-all-us-doctors-get-money-from-pharma-each-year/ "Go to: Report: More than half of all US doctors get money from pharma each year")

1. [![Listing image for first story in Most Read: Review: Yes, we're still arguing about Nolan's The Odyssey](https://cdn.arstechnica.net/wp-content/uploads/2026/07/odysseyTOP-500x500.jpg)](https://arstechnica.com/culture/2026/08/review-yes-were-still-arguing-about-nolans-the-odyssey/)




1. [Review: Yes, we're still arguing about Nolan's The Odyssey](https://arstechnica.com/culture/2026/08/review-yes-were-still-arguing-about-nolans-the-odyssey/)

2. 2. [As Reddit stock falls, CEO questions value of Google's AI Overviews](https://arstechnica.com/ai/2026/08/reddit-ceo-on-ai-overviews-were-still-looking-for-that-win-win/)

3. 3. [How headlights got brighter, whiter, and more blinding after dark](https://arstechnica.com/cars/2026/08/how-headlights-got-brighter-whiter-and-more-blinding-after-dark/)

4. 4. [Here's how engineers plan to save the satellite sent to save NASA's Swift mission](https://arstechnica.com/space/2026/08/heres-how-engineers-plan-to-save-the-satellite-sent-to-save-nasas-swift-mission/)

5. 5. [How a Yale AI-cheating dispute became a 13-count federal lawsuit](https://arstechnica.com/tech-policy/2026/07/how-a-yale-ai-cheating-dispute-became-a-13-count-federal-lawsuit/)


Customize

Sign in dialog...

──────── [TRUNCATED] ────────
Showing 29,787 chars (head) + 9,789 chars (tail) of 94,822 total clean characters.
Full text saved to: /home/wubu/.hermes/profiles/mind-palace/cache/web/arstechnica.com-84402e3496.md
To read the omitted middle: read_file path="/home/wubu/.hermes/profiles/mind-palace/cache/web/arstechnica.com-84402e3496.md" offset=196 limit=200  (the file is the complete page; raise/lower offset to page through it).
─────────────────────────────