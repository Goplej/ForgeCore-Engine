#pragma once
// ForgeCore streaming channel — connects the headless engine to the
// browser preview:
//   /dev/shm/fc_frame.rgb  - latest frame (RGB8, written by the engine)
//   /dev/shm/fc_input      - JSON lines appended by the preview server
//                            (key / mouse / scroll), drained by the engine
//   /dev/shm/fc_events     - JSON lines appended by the engine
//                            (audio + stats), tailed by the preview server

#include "fc/input.hpp"
#include <cstdint>
#include <string>

namespace fc {

class StreamChannel {
 public:
  // dir defaults to /dev/shm. Returns false if the dir is not usable.
  bool init(const std::string& dir, int width, int height);
  void shutdown();

  void publish_frame(const uint8_t* rgb8);
  void drain_input(Input& input);
  void append_event(const std::string& json);

 private:
  std::string frame_path_, input_path_, events_path_;
  long input_off_ = 0;
  int w_ = 0, h_ = 0;
};

}  // namespace fc
