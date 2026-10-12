#pragma once
// ======================================================================
// connectivity/net_addr.h
// EN: Chooses destination addresses (broadcast / unicast) for frames to AgOpenWeb from the actual subnet (phone
//     hotspots use random subnets).
// PL: Wybiera adresy docelowe (broadcast / unicast) ramek do AgOpenWeb z faktycznej podsieci (hotspot telefonu
//     ma losową podsieć).
// Tests: tests/test_net_addr.cpp (host, plain C++).
// Used by: connectivity/pgn_messages.h
// ======================================================================
// Choosing network addresses for frames sent to AgOpenWeb. Pure C++ (no Arduino) - host-tested (tests/test_net_addr.cpp).
// Addresses are uint32_t in ANY, but consistent, byte order (IPAddress cast to uint32_t): bitwise AND/OR/NOT on an address
// and a mask are correct regardless of the order, because all arguments share the same representation.
//
// WHY: the module used to hard-code the home address (192.168.1.x) and broadcast 192.168.1.255. A phone hotspot gives a completely different, random subnet
// (e.g. 10.20.255.x with gateway 10.20.255.117, 192.168.43.x, or 172.20.10.x with a /28 mask on iPhone) - frames went "into space".
#include <cstdint>

namespace NetAddr {
    // Subnet broadcast address: the network part + all ones in the host part (any contiguous mask: /24, /28, /20...).
    inline uint32_t broadcast(uint32_t ip, uint32_t mask) { return (ip & mask) | ~mask; }

    // Whether two addresses are in the same subnet (mask 0 = no network information => false).
    inline bool sameSubnet(uint32_t a, uint32_t b, uint32_t mask) { return mask != 0 && ((a & mask) == (b & mask)); }

    // Where to send frames to AgOpenWeb:
    //   - no network information (address 0 or mask 0: e.g. AP mode, not connected yet) -> as before (learned or configured),
    //   - the learned AgOpenWeb address IS in the current subnet -> unicast to it,
    //   - the learned address is from a DIFFERENT subnet (learned on a previous network, e.g. at home) -> IGNORE it (stale),
    //   - the configured address (AGIO_IP) is in the current subnet -> use it,
    //   - otherwise the broadcast of the CURRENT subnet.
    inline uint32_t pickDest(bool known, uint32_t knownIp, uint32_t configuredAgio, uint32_t myIp, uint32_t mask) {
        if (myIp == 0 || mask == 0) return known ? knownIp : configuredAgio;
        if (known && sameSubnet(knownIp, myIp, mask)) return knownIp;
        if (sameSubnet(configuredAgio, myIp, mask)) return configuredAgio;
        return broadcast(myIp, mask);
    }
}
