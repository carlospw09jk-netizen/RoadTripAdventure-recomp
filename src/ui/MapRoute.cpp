// The map router's decision (MapRouter.h), on its own: no runtime, so map_router_test can link it.

#include "ui/MapRouter.h"

namespace rt::ui::maprouter
{
    const char *name(Destination d)
    {
        switch (d)
        {
        case Destination::SecondScreen: return "second";
        case Destination::Overlay: return "overlay";
        default: return "game";
        }
    }

    Destination decide(State &s, const Inputs &in)
    {
        // The clocks start at the first call, and again when guest time goes back (a state load).
        if (!s.started || in.vblank < s.lastVblank)
        {
            const bool seen = s.started && s.lastSeen != in.framesSeen;
            s.lastSeen = in.framesSeen, s.lastSerial = in.serial;
            s.lastSeenAt = seen ? in.vblank : 0;
            s.lastSerialAt = s.sentSince = in.vblank;
            s.secondLost = false;
            s.barredUntil = 0;
            s.started = true;
        }
        s.lastVblank = in.vblank;
        if (in.framesSeen != s.lastSeen)
            s.lastSeen = in.framesSeen, s.lastSeenAt = in.vblank, s.seen = true;
        if (in.serial != s.lastSerial)
            s.lastSerial = in.serial, s.lastSerialAt = in.vblank;

        // A second screen whose swapchain went away (a surface rebuild, out of date) keeps the map
        // for a moment: it usually comes back at once.
        if (in.secondScreenOn && !in.secondSwapchain)
        {
            if (!s.secondLost)
                s.secondLost = true, s.secondLostAt = in.vblank;
        }
        else
            s.secondLost = false;
        const bool second = in.secondScreenOn && (!s.secondLost || in.vblank - s.secondLostAt < kSecondGrace);

        Destination d = Destination::Game;
        if (in.mapLayer && !in.demo && in.vblank >= s.barredUntil)
        {
            if (second && !in.playerHandedAway)
                d = Destination::SecondScreen;
            else if (in.enhanced)
                d = Destination::Overlay;
        }
        if (d != Destination::Game)
        {
            if (s.dest == Destination::Game)
                s.sentSince = in.vblank, s.lastSerialAt = in.vblank;
            // Asked for, drawn by the game, and yet nothing published for a while: the map would
            // be on neither screen. The game keeps it for a while.
            const bool drawing = s.seen && in.vblank - s.lastSeenAt <= 10;
            if (drawing && in.vblank - s.sentSince > kUnpublished && in.vblank - s.lastSerialAt > kUnpublished)
            {
                s.barredUntil = in.vblank + kBar;
                d = Destination::Game;
            }
        }
        s.dest = d;
        return d;
    }

    bool mapRecent(const State &s, uint64_t vblank) { return s.seen && vblank >= s.lastSeenAt && vblank - s.lastSeenAt <= kRecent; }

}
