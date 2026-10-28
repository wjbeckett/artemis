#pragma once

// A permutation keeps each host modifier available and avoids two client keys
// sharing one host key-down state. The indices are also the settings UI order.
struct ModifierMapping
{
    enum Target { Control, Alt, Meta, Count };
    int targets[Count];

    ModifierMapping(int control = Control, int option = Alt, int command = Meta)
        : targets{control, option, command}
    {
        if (!isValid()) {
            targets[Control] = Control;
            targets[Alt] = Alt;
            targets[Meta] = Meta;
        }
    }

    bool isValid() const
    {
        unsigned int seen = 0;
        for (int target : targets) {
            if (target < 0 || target >= Count || (seen & (1u << target))) {
                return false;
            }
            seen |= 1u << target;
        }
        return true;
    }

    bool isIdentity() const
    {
        return targets[Control] == Control && targets[Alt] == Alt && targets[Meta] == Meta;
    }

    bool setTarget(int source, int target)
    {
        if (source < 0 || source >= Count || target < 0 || target >= Count || targets[source] == target) {
            return false;
        }
        for (int other = 0; other < Count; other++) {
            if (targets[other] == target) {
                targets[other] = targets[source];
                targets[source] = target;
                return true;
            }
        }
        return false;
    }
};
