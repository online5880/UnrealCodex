# UnrealMasterAI → UnrealCodex 이식 트래커

- 생성시각: 2026-03-12 14:22:55
- 기준 타겟: UnrealCodex (C++ 플러그인)
- 소스 기준: UnrealMasterAI (Python scripts + MCP domains)

## 1) 현재 이식 완료 항목 (체크됨)
- [x] MCP Hook 파이프라인 도입 (pre/post hook)
- [x] Validation Hook (required 파라미터 검증)
- [x] Safety Hook + 승인 모달 연동 (ScriptPermissionDialog)
- [x] SourceControl 통합 툴: `source_control` (status/checkout/diff)
- [x] SourceControl 분리 툴: `source_control_status`, `source_control_checkout`, `source_control_diff`
- [x] source_control 응답 표준화 + diff `max_chars` 지원
- [x] Project context 조회 툴 추가: `project_context` (summary/full)

### 관련 커밋
- `b84fe79`
- `28a974d`
- `26152f2`
- `54036a3`
- `6685ac8`
- `c4193e7`
- `02612ca`
- `afee926`

## 2) UnrealMasterAI Python 스크립트 인벤토리
- 총 스크립트 수: **165**

| 도메인(prefix) | 개수 | 상태 | 비고 |
|---|---:|---|---|
| `actor` | 8 | 미이식 | 배치 대상 |
| `ai` | 8 | 미이식 | 배치 대상 |
| `asset` | 8 | 미이식 | 배치 대상 |
| `sequencer` | 8 | 미이식 | 배치 대상 |
| `workflow` | 8 | 미이식 | 배치 대상 |
| `audio` | 6 | 미이식 | 배치 대상 |
| `level` | 6 | 미이식 | 배치 대상 |
| `material` | 6 | 미이식 | 배치 대상 |
| `niagara` | 6 | 미이식 | 배치 대상 |
| `project` | 6 | 미이식 | 배치 대상 |
| `texture` | 6 | 미이식 | 배치 대상 |
| `widget` | 6 | 미이식 | 배치 대상 |
| `anim` | 5 | 미이식 | 배치 대상 |
| `debug` | 5 | 미이식 | 배치 대상 |
| `editor` | 5 | 미이식 | 배치 대상 |
| `landscape` | 5 | 미이식 | 배치 대상 |
| `physics` | 5 | 미이식 | 배치 대상 |
| `setup` | 5 | 미이식 | 배치 대상 |
| `analyze` | 4 | 미이식 | 배치 대상 |
| `content` | 4 | 미이식 | 배치 대상 |
| `datatable` | 4 | 미이식 | 배치 대상 |
| `gameplay` | 4 | 미이식 | 배치 대상 |
| `mesh` | 4 | 미이식 | 배치 대상 |
| `pcg` | 4 | 미이식 | 배치 대상 |
| `worldpartition` | 4 | 미이식 | 배치 대상 |
| `build` | 3 | 미이식 | 배치 대상 |
| `curve` | 3 | 미이식 | 배치 대상 |
| `foliage` | 3 | 미이식 | 배치 대상 |
| `geoscript` | 3 | 미이식 | 배치 대상 |
| `sourcecontrol` | 3 | 미이식 | 배치 대상 |
| `add` | 2 | 미이식 | 배치 대상 |
| `fix` | 2 | 미이식 | 배치 대상 |
| `apply` | 1 | 미이식 | 배치 대상 |
| `compile` | 1 | 미이식 | 배치 대상 |
| `context` | 1 | 미이식 | 배치 대상 |
| `detect` | 1 | 미이식 | 배치 대상 |
| `misc` | 1 | 미이식 | 배치 대상 |
| `refactor` | 1 | 미이식 | 배치 대상 |

## 3) 저위험 배치(읽기/조회 중심) 후보
- 후보 수: **56**

- [ ] `ai_get_behavior_tree_info.py`
- [ ] `ai_get_blackboard_keys.py`
- [ ] `ai_get_nav_mesh_info.py`
- [ ] `analyze_asset_health.py`
- [ ] `analyze_blueprint_complexity.py`
- [ ] `analyze_code_conventions.py`
- [ ] `analyze_performance_hints.py`
- [ ] `anim_list_montages.py`
- [ ] `anim_list_sequences.py`
- [ ] `anim_skeleton_info.py`
- [ ] `audio_get_info.py`
- [ ] `audio_list_assets.py`
- [ ] `content_details.py`
- [ ] `content_find.py`
- [ ] `content_list.py`
- [ ] `content_validate.py`
- [ ] `curve_get_info.py`
- [ ] `datatable_get_rows.py`
- [ ] `debug_log.py`
- [ ] `debug_material_api.py`
- [ ] `debug_performance.py`
- [ ] `debug_subobject.py`
- [ ] `debug_subobject2.py`
- [ ] `detect_conventions.py`
- [ ] `editor_get_recent_activity.py`
- [ ] `editor_get_selection.py`
- [ ] `editor_get_viewport.py`
- [ ] `foliage_get_info.py`
- [ ] `gameplay_get_mode.py`
- [ ] `gameplay_list_inputs.py`
- [ ] `geoscript_get_info.py`
- [ ] `landscape_get_info.py`
- [ ] `material_get_nodes.py`
- [ ] `material_get_params.py`
- [ ] `mesh_info.py`
- [ ] `niagara_get_info.py`
- [ ] `niagara_list_systems.py`
- [ ] `pcg_get_info.py`
- [ ] `physics_get_info.py`
- [x] `project_class_hierarchy.py` (대응: `project_inspect` action=class_hierarchy)
- [x] `project_dependencies.py` (대응: `project_inspect` action=dependencies)
- [x] `project_plugins.py` (대응: `project_inspect` action=plugins)
- [x] `project_settings.py` (대응: `project_inspect` action=settings)
- [x] `project_snapshot.py` (대응: `project_context` full)
- [x] `project_structure.py` (대응: `project_context` summary/full)
- [ ] `sequencer_get_info.py`
- [x] `sourcecontrol_status.py` (대응: `source_control_status`)
- [ ] `texture_get_info.py`
- [ ] `texture_list_textures.py`
- [ ] `widget_add_element.py`
- [ ] `widget_create.py`
- [ ] `widget_get_bindings.py`
- [ ] `widget_get_info.py`
- [ ] `widget_list_widgets.py`
- [ ] `widget_set_property.py`
- [ ] `worldpartition_get_info.py`

## 4) 중위험 배치 후보
- 후보 수: **48**

- [ ] `actor_add_component.py`
- [ ] `actor_components.py`
- [ ] `actor_properties.py`
- [ ] `actor_select.py`
- [ ] `actor_spawn.py`
- [ ] `actor_transform.py`
- [ ] `add_light_to_spinning_cube.py`
- [ ] `add_spinning_sound.py`
- [ ] `ai_add_blackboard_key.py`
- [ ] `anim_blend_space.py`
- [ ] `apply_random_color_material.py`
- [ ] `context_auto_gather.py`
- [ ] `curve_create.py`
- [ ] `datatable_add_row.py`
- [ ] `datatable_create.py`
- [ ] `editor_batch_operation.py`
- [ ] `fix_character_setup.py`
- [ ] `fix_random_color_material.py`
- [ ] `gameplay_add_input.py`
- [ ] `geoscript_mesh_boolean.py`
- [ ] `geoscript_mesh_transform.py`
- [ ] `landscape_create.py`
- [ ] `level_create.py`
- [ ] `level_open.py`
- [ ] `level_save.py`
- [ ] `level_sublevel.py`
- [ ] `level_world_settings.py`
- [ ] `material_create.py`
- [ ] `mesh_collision.py`
- [ ] `mesh_lod.py`
- [ ] `niagara_add_emitter.py`
- [ ] `pcg_add_node.py`
- [ ] `pcg_connect_nodes.py`
- [ ] `sequencer_add_binding.py`
- [ ] `sequencer_add_track.py`
- [ ] `sequencer_create.py`
- [ ] `sequencer_open.py`
- [ ] `setup_game_hud.py`
- [ ] `setup_patrol_movement.py`
- [ ] `setup_patrol_system.py`
- ... (생략 8개)

## 5) 고위험 배치(파괴/대량변경 가능) 후보
- 후보 수: **61**

- [ ] `actor_delete.py`
- [ ] `actor_set_property.py`
- [ ] `ai_configure_nav_mesh.py`
- [ ] `ai_create_behavior_tree.py`
- [ ] `ai_create_blackboard.py`
- [ ] `ai_create_eqs.py`
- [ ] `anim_create_montage.py`
- [ ] `asset_create.py`
- [ ] `asset_delete.py`
- [ ] `asset_duplicate.py`
- [ ] `asset_export.py`
- [ ] `asset_import.py`
- [ ] `asset_metadata.py`
- [ ] `asset_references.py`
- [ ] `asset_rename.py`
- [ ] `audio_create_cue.py`
- [ ] `audio_create_meta_sound.py`
- [ ] `audio_import.py`
- [ ] `audio_set_attenuation.py`
- [ ] `build_cook.py`
- [ ] `build_lightmaps.py`
- [ ] `build_map_check.py`
- [ ] `compile_game_hud.py`
- [ ] `curve_set_keys.py`
- [ ] `datatable_remove_row.py`
- [ ] `editor_set_selection.py`
- [ ] `foliage_create_type.py`
- [ ] `foliage_set_properties.py`
- [ ] `gameplay_set_mode.py`
- [ ] `landscape_export_heightmap.py`
- [ ] `landscape_import_heightmap.py`
- [ ] `landscape_set_material.py`
- [ ] `level_clear_actors.py`
- [ ] `material_create_instance.py`
- [ ] `material_set_param.py`
- [ ] `material_set_texture.py`
- [ ] `mesh_set_material.py`
- [ ] `niagara_compile.py`
- [ ] `niagara_create_system.py`
- [ ] `niagara_set_parameter.py`
- [ ] `pcg_create_graph.py`
- [ ] `physics_create_asset.py`
- [ ] `physics_create_material.py`
- [ ] `physics_set_constraint.py`
- [ ] `physics_set_profile.py`
- [ ] `refactor_rename_chain.py`
- [ ] `sequencer_export_fbx.py`
- [ ] `sequencer_import_fbx.py`
- [ ] `sequencer_set_keyframe.py`
- [ ] `texture_create_render_target.py`
- [ ] `texture_import.py`
- [ ] `texture_set_compression.py`
- [ ] `workflow_create_character.py`
- [ ] `workflow_create_dialogue_system.py`
- [ ] `workflow_create_interactable.py`
- [ ] `workflow_create_inventory_system.py`
- [ ] `workflow_create_projectile.py`
- [ ] `workflow_create_ui_screen.py`
- [ ] `worldpartition_create_data_layer.py`
- [ ] `worldpartition_create_hlod.py`
- [ ] `worldpartition_set_config.py`

## 6) 진행 규칙
1. 저위험 배치부터 이식하고 체크박스 업데이트
2. 중/고위험은 Safety Hook + 승인 모달 검증 후 진행
3. 각 배치마다 커밋 해시를 문서 하단 `변경 이력`에 추가
4. UnrealCodex 빌드/런타임 검증 통과 시에만 완료 체크

## 7) 변경 이력
- 초기 문서 생성
- project_inspect 확장: `dependencies`, `class_hierarchy` 액션 추가 (project_dependencies.py / project_class_hierarchy.py 대응)
