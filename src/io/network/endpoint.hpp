// Copyright 2023 Memgraph Ltd.
//
// Use of this software is governed by the Business Source License
// included in the file licenses/BSL.txt; by using this file, you agree to be bound by the terms of the Business Source
// License, and you may not use this file except in compliance with the Business Source License.
//
// As of the Change Date specified in that file, in accordance with
// the Business Source License, use of this software will be governed
// by the Apache License, Version 2.0, included in the file
// licenses/APL.txt.

#pragma once

#include <netinet/in.h>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace memgraph::io::network {

/**
 * This class represents a network endpoint that is used in Socket.
 * It is used when connecting to an address and to get the current
 * connection address.
 */
struct Endpoint {
  Endpoint() = default;
  Endpoint(std::string ip_address, uint16_t port);
  Endpoint(Endpoint const &) = default;
  Endpoint(Endpoint &&) noexcept = default;
  Endpoint &operator=(Endpoint const &) = default;
  Endpoint &operator=(Endpoint &&) noexcept = default;
  ~Endpoint() = default;

  enum class IpFamily : std::uint8_t { NONE, IP4, IP6 };

  std::string SocketAddress() const;

  bool operator==(const Endpoint &other) const = default;
  friend std::ostream &operator<<(std::ostream &os, const Endpoint &endpoint);

  std::string address;
  uint16_t port{0};
  IpFamily family{IpFamily::NONE};

  /**
   * Tries to parse the given string as either a socket address or ip address.
   * Expected address format:
   *   - "ip_address:port_number"
   *   - "ip_address"
   * We parse the address first. If it's an IP address, a default port must
   * be given, or we return nullopt. If it's a socket address, we try to parse
   * it into an ip address and a port number; even if a default port is given,
   * it won't be used, as we expect that it is given in the address string.
   */
  static std::optional<std::pair<std::string, uint16_t>> ParseSocketOrIpAddress(
      const std::string &address, const std::optional<uint16_t> default_port);

  static IpFamily GetIpFamily(const std::string &ip_address);
};

}  // namespace memgraph::io::network
