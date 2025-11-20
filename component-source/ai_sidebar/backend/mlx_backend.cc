// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)

#include "components/ai_sidebar/backend/mlx_backend.h"

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"

namespace ai_sidebar {

namespace {

constexpr char kMLXPythonScript[] = R"(
import sys
import json
import mlx.core as mx
from mlx_lm import load, generate

try:
    config = json.loads(sys.argv[1])
    model_path = config['model']
    prompt = config['prompt']

    model, tokenizer = load(model_path)

    response = generate(model, tokenizer, prompt=prompt,
                       max_tokens=config.get('max_tokens', 512),
                       temp=config.get('temperature', 0.7))

    print(json.dumps({'success': True, 'text': response}))
except Exception as e:
    print(json.dumps({'success': False, 'error': str(e)}))
)";

constexpr char kMLXVisionScript[] = R"(
import sys
import json
import mlx.core as mx
from mlx_vlm import load, generate

try:
    config = json.loads(sys.argv[1])
    model_path = config['model']
    prompt = config['prompt']
    image_path = config.get('image_path', '')

    model, processor = load(model_path)

    if image_path:
        from PIL import Image
        image = Image.open(image_path)
        response = generate(model, processor, image, prompt,
                          max_tokens=config.get('max_tokens', 512))
    else:
        response = generate(model, processor, prompt=prompt,
                          max_tokens=config.get('max_tokens', 512))

    print(json.dumps({'success': True, 'text': response}))
except Exception as e:
    print(json.dumps({'success': False, 'error': str(e)}))
)";

base::FilePath GetDefaultModelDirectory() {
  base::FilePath home_dir;
  base::PathService::Get(base::DIR_HOME, &home_dir);
  return home_dir.Append(".cache/huggingface/hub");
}

}  // namespace

MLXBackend::MLXBackend()
    : model_directory_(GetDefaultModelDirectory()),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})) {}

MLXBackend::~MLXBackend() = default;

bool MLXBackend::Initialize() {
  is_available_ = CheckMLXAvailability();
  if (is_available_) {
    ScanForModels();
  }
  return is_available_;
}

bool MLXBackend::IsAvailable() const {
  return is_available_;
}

void MLXBackend::GenerateText(const GenerationRequest& request,
                             GenerationCallback callback) {
  if (!IsAvailable()) {
    GenerationResult result;
    result.success = false;
    result.error_message = "MLX is not available on this system";
    std::move(callback).Run(std::move(result));
    return;
  }

  // Create configuration for Python script
  base::Value::Dict config;

  // Extract model path from model ID
  std::string model_name = request.model;
  if (base::StartsWith(model_name, "mlx:")) {
    model_name = model_name.substr(4);
  }

  // Find model path
  base::FilePath model_path;
  for (const auto& model : discovered_models_) {
    if (model.id == request.model || model.name == model_name) {
      model_path = model.path;
      break;
    }
  }

  if (model_path.empty()) {
    GenerationResult result;
    result.success = false;
    result.error_message = "Model not found: " + model_name;
    std::move(callback).Run(std::move(result));
    return;
  }

  config.Set("model", model_path.value());
  config.Set("prompt", request.prompt);
  config.Set("max_tokens", 512);
  config.Set("temperature", 0.7);

  // Handle images for vision models
  if (!request.image_data.empty()) {
    // Save image to temp file
    base::FilePath temp_dir;
    base::PathService::Get(base::DIR_TEMP, &temp_dir);
    base::FilePath image_path = temp_dir.Append("mlx_input_image.jpg");

    if (base::WriteFile(image_path,
                       reinterpret_cast<const char*>(request.image_data.data()),
                       request.image_data.size())) {
      config.Set("image_path", image_path.value());
    }
  }

  std::string config_json;
  base::JSONWriter::Write(config, &config_json);

  // Execute MLX script
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](std::string script, std::string config_json,
             bool has_image) -> std::string {
            base::CommandLine command(base::FilePath("/usr/bin/python3"));
            command.AppendArg("-c");
            command.AppendArg(has_image ? kMLXVisionScript : kMLXPythonScript);
            command.AppendArg(config_json);

            std::string output;
            base::GetAppOutput(command, &output);
            return output;
          },
          kMLXPythonScript, config_json, !request.image_data.empty()),
      base::BindOnce(&MLXBackend::OnGenerateComplete, base::Unretained(this),
                     std::move(callback)));
}

void MLXBackend::GenerateTextStreaming(
    const GenerationRequest& request,
    StreamCallback stream_callback,
    GenerationCallback complete_callback) {
  // For now, fall back to non-streaming
  GenerateText(request, std::move(complete_callback));
}

void MLXBackend::ListModels(ModelsCallback callback) {
  std::vector<ModelInfo> models;

  for (const auto& model_meta : discovered_models_) {
    ModelInfo info;
    info.id = model_meta.id;
    info.display_name = model_meta.name + " (MLX)";
    info.provider = "mlx";
    info.capabilities = model_meta.capabilities;
    info.context_length = 2048;  // Default
    info.description = "Local MLX model on Apple Silicon";
    models.push_back(std::move(info));
  }

  std::move(callback).Run(models);
}

std::string MLXBackend::GetBackendName() const {
  return "mlx";
}

bool MLXBackend::SupportsCapability(const std::string& model_id,
                                   ModelCapability capability) const {
  for (const auto& model : discovered_models_) {
    if (model.id == model_id) {
      return (model.capabilities & static_cast<uint32_t>(capability)) != 0;
    }
  }
  return false;
}

void MLXBackend::SetModelDirectory(const base::FilePath& directory) {
  model_directory_ = directory;
  if (is_available_) {
    ScanForModels();
  }
}

void MLXBackend::ScanForModels() {
  discovered_models_.clear();

  if (!base::PathExists(model_directory_)) {
    return;
  }

  // Scan for model directories
  base::FileEnumerator enumerator(model_directory_, false,
                                  base::FileEnumerator::DIRECTORIES);

  for (base::FilePath model_dir = enumerator.Next(); !model_dir.empty();
       model_dir = enumerator.Next()) {
    // Check if directory contains model files
    base::FilePath config_file = model_dir.Append("config.json");
    if (!base::PathExists(config_file)) {
      continue;
    }

    std::string config_content;
    if (!base::ReadFileToString(config_file, &config_content)) {
      continue;
    }

    auto config = base::JSONReader::Read(config_content);
    if (!config || !config->is_dict()) {
      continue;
    }

    ModelMetadata metadata;
    metadata.path = model_dir;
    metadata.name = model_dir.BaseName().value();
    metadata.id = "mlx:" + metadata.name;
    metadata.capabilities = static_cast<uint32_t>(ModelCapability::TEXT);

    // Check for vision capabilities
    const std::string* model_type =
        config->GetDict().FindString("model_type");
    if (model_type && (base::Contains(*model_type, "vision") ||
                      base::Contains(*model_type, "llava"))) {
      metadata.capabilities |= static_cast<uint32_t>(ModelCapability::IMAGE);
    }

    // Check for audio capabilities
    if (model_type && (base::Contains(*model_type, "whisper") ||
                      base::Contains(*model_type, "audio"))) {
      metadata.capabilities |= static_cast<uint32_t>(ModelCapability::AUDIO);
    }

    discovered_models_.push_back(std::move(metadata));
  }
}

void MLXBackend::OnGenerateComplete(GenerationCallback callback,
                                   std::string output) {
  GenerationResult result;

  auto parsed = base::JSONReader::Read(output);
  if (!parsed || !parsed->is_dict()) {
    result.success = false;
    result.error_message = "Failed to parse MLX output";
    std::move(callback).Run(std::move(result));
    return;
  }

  auto success = parsed->GetDict().FindBool("success");
  result.success = success.value_or(false);

  if (result.success) {
    const std::string* text = parsed->GetDict().FindString("text");
    result.text = text ? *text : "";
  } else {
    const std::string* error = parsed->GetDict().FindString("error");
    result.error_message = error ? *error : "Unknown error";
  }

  std::move(callback).Run(std::move(result));
}

bool MLXBackend::CheckMLXAvailability() {
  // Check if we're on Apple Silicon
  base::CommandLine command(base::FilePath("/usr/bin/uname"));
  command.AppendArg("-m");

  std::string output;
  if (!base::GetAppOutput(command, &output)) {
    return false;
  }

  // Check for arm64 (Apple Silicon)
  if (!base::Contains(output, "arm64")) {
    return false;
  }

  // Check if MLX is installed
  base::CommandLine python_command(base::FilePath("/usr/bin/python3"));
  python_command.AppendArg("-c");
  python_command.AppendArg("import mlx.core; import mlx_lm");

  return base::GetAppOutput(python_command, &output);
}

}  // namespace ai_sidebar

#endif  // BUILDFLAG(IS_MAC)
