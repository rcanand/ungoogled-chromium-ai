// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_MLX_BACKEND_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_MLX_BACKEND_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)

#include "components/ai_sidebar/backend/ai_backend_interface.h"
#include "base/files/file_path.h"
#include "base/process/process.h"

namespace base {
class SequencedTaskRunner;
}

namespace ai_sidebar {

// MLX backend for local model inference on macOS
// Uses Apple's MLX framework for efficient inference on Apple Silicon
class MLXBackend : public AIBackendInterface {
 public:
  MLXBackend();
  ~MLXBackend() override;

  // AIBackendInterface implementation
  bool Initialize() override;
  bool IsAvailable() const override;
  void GenerateText(const GenerationRequest& request,
                   GenerationCallback callback) override;
  void GenerateTextStreaming(const GenerationRequest& request,
                            StreamCallback stream_callback,
                            GenerationCallback complete_callback) override;
  void ListModels(ModelsCallback callback) override;
  std::string GetBackendName() const override;
  bool SupportsCapability(const std::string& model_id,
                         ModelCapability capability) const override;

  // Set custom model directory
  void SetModelDirectory(const base::FilePath& directory);

 private:
  struct ModelMetadata {
    std::string id;
    std::string name;
    uint32_t capabilities;
    base::FilePath path;
  };

  void ScanForModels();
  void ExecuteMLXScript(const std::string& script_name,
                       const base::Value::Dict& args,
                       base::OnceCallback<void(std::string)> callback);
  void OnGenerateComplete(GenerationCallback callback,
                         std::string output);
  void OnModelScanComplete(ModelsCallback callback,
                          std::string output);

  bool CheckMLXAvailability();
  std::string CreateGenerateScript(const GenerationRequest& request);

  bool is_available_ = false;
  base::FilePath model_directory_;
  std::vector<ModelMetadata> discovered_models_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace ai_sidebar

#endif  // BUILDFLAG(IS_MAC)

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_MLX_BACKEND_H_
