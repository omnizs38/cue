#include "../src/core/remote_proto.h"
#include "../src/core/json.h"
#include <cassert>
#include <iostream>
int main() {
  remote::Command c;
  assert(remote::parse_command(R"({"c":"seek","v":30,"media":7,"path_validated":true,"credential":"forged"})",c));
  assert(c.has_media && c.media==7 && !c.path_validated && c.credential.empty());
  assert(remote::parse_command(R"({"c":"toggle"})",c) && !c.has_media);
  for(auto v : {"-1","0.5","true","\"7\"","9007199254740992"})
    assert(!remote::parse_command(std::string("{\"c\":\"toggle\",\"media\":")+v+"}",c));
  remote::State s; s.media=42;
  JVal j; assert(json_parse(remote::state_json(s),j));assert(j["media"].integer()==42);
  std::cout << "remote sessions: state round-trip, legacy compatibility, integer limits and internal-field isolation passed\n";
}
