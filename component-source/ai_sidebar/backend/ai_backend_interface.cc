// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/backend/ai_backend_interface.h"

namespace ai_sidebar {

base::Value::Dict ModelInfo::ToDict() const {
  base::Value::Dict dict;
  dict.Set("id", id);
  dict.Set("display_name", display_name);
  dict.Set("provider", provider);
  dict.Set("capabilities", static_cast<int>(capabilities));
  dict.Set("context_length", static_cast<int>(context_length));
  dict.Set("description", description);
  return dict;
}

// static
ModelInfo ModelInfo::FromDict(const base::Value::Dict& dict) {
  ModelInfo info;
  if (const std::string* id_val = dict.FindString("id"))
    info.id = *id_val;
  if (const std::string* name = dict.FindString("display_name"))
    info.display_name = *name;
  if (const std::string* provider = dict.FindString("provider"))
    info.provider = *provider;
  if (auto cap = dict.FindInt("capabilities"))
    info.capabilities = static_cast<uint32_t>(*cap);
  if (auto len = dict.FindInt("context_length"))
    info.context_length = static_cast<size_t>(*len);
  if (const std::string* desc = dict.FindString("description"))
    info.description = *desc;
  return info;
}

}  // namespace ai_sidebar
