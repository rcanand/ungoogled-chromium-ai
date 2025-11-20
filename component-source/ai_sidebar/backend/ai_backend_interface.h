// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_AI_BACKEND_INTERFACE_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_AI_BACKEND_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>
#include "base/functional/callback.h"
#include "base/values.h"

namespace ai_sidebar {

enum class ModelCapability {
  TEXT = 1 << 0,
  IMAGE = 1 << 1,
  AUDIO = 1 << 2,
};

struct ModelInfo {
  std::string id;
  std::string display_name;
  std::string provider;
  uint32_t capabilities;  // Bitfield of ModelCapability
  size_t context_length;
  std::string description;

  base::Value::Dict ToDict() const;
  static ModelInfo FromDict(const base::Value::Dict& dict);
};

struct GenerationRequest {
  std::string prompt;
  std::string model;
  std::string system_prompt;
  std::vector<uint8_t> image_data;  // Optional for vision models
  std::vector<uint8_t> audio_data;  // Optional for audio models
  base::Value::Dict extra_params;
};

struct GenerationResult {
  std::string text;
  bool success;
  std::string error_message;
  base::Value::Dict metadata;
};

using GenerationCallback = base::OnceCallback<void(GenerationResult)>;
using StreamCallback = base::RepeatingCallback<void(const std::string& chunk)>;
using ModelsCallback = base::OnceCallback<void(std::vector<ModelInfo>)>;

// Abstract interface for AI backends
class AIBackendInterface {
 public:
  virtual ~AIBackendInterface() = default;

  // Initialize the backend
  virtual bool Initialize() = 0;

  // Check if backend is available on this platform
  virtual bool IsAvailable() const = 0;

  // Generate text response
  virtual void GenerateText(const GenerationRequest& request,
                           GenerationCallback callback) = 0;

  // Generate text response with streaming
  virtual void GenerateTextStreaming(const GenerationRequest& request,
                                    StreamCallback stream_callback,
                                    GenerationCallback complete_callback) = 0;

  // List available models
  virtual void ListModels(ModelsCallback callback) = 0;

  // Get backend name
  virtual std::string GetBackendName() const = 0;

  // Check if model supports specific capability
  virtual bool SupportsCapability(const std::string& model_id,
                                 ModelCapability capability) const = 0;
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_AI_BACKEND_INTERFACE_H_
