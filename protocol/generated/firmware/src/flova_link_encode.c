/*
 * Generated using zcbor version 0.9.1
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 1
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "flova_link_encode.h"
#include "zcbor_print.h"

#if DEFAULT_MAX_QTY != 1
#error "The type file was generated with a different default_max_qty than this file"
#endif

#define log_result(state, result, func) do { \
	if (!result) { \
		zcbor_trace_file(state); \
		zcbor_log("%s error: %s\r\n", func, zcbor_error_str(zcbor_peek_error(state))); \
	} else { \
		zcbor_log("%s success\r\n", func); \
	} \
} while(0)

static bool encode_repeated_ota_desired_ota_target(zcbor_state_t *state, const struct ota_desired_ota_target *input);
static bool encode_repeated_ota_desired_ota_release_id(zcbor_state_t *state, const struct ota_desired_ota_release_id *input);
static bool encode_repeated_datastream_binding_keys_tstr1_48(zcbor_state_t *state, const struct zcbor_string *input);
static bool encode_datastream_binding_keys(zcbor_state_t *state, const struct datastream_binding_keys *input);
static bool encode_repeated_guardrail_report_guardrail_incident_l(zcbor_state_t *state, const struct guardrail_report_guardrail_incident_l *input);
static bool encode_typed_value_fields(zcbor_state_t *state, const struct typed_value_fields_r *input);
static bool encode_correlation_id(zcbor_state_t *state, const struct correlation_id_r *input);
static bool encode_repeated_command_guardrail_latch(zcbor_state_t *state, const uint64_t *input);
static bool encode_repeated_capabilities_pin_references(zcbor_state_t *state, const struct capabilities_pin_references *input);
static bool encode_repeated_capabilities_offline_guardrail_slots(zcbor_state_t *state, const struct capabilities_offline_guardrail_slots *input);
static bool encode_capabilities(zcbor_state_t *state, const struct capabilities *input);
static bool encode_repeated_datastream_bound_ids_compact_id_m(zcbor_state_t *state, const uint64_t *input);
static bool encode_datastream_bound_ids(zcbor_state_t *state, const struct datastream_bound_ids *input);
static bool encode_scheduling_stream(zcbor_state_t *state, const struct scheduling_stream *input);
static bool encode_repeated_ota_profile_ota_boot_state(zcbor_state_t *state, const struct ota_profile_ota_boot_state *input);
static bool encode_repeated_ota_profile_ota_rollback_reason(zcbor_state_t *state, const struct ota_profile_ota_rollback_reason *input);
static bool encode_ota_profile(zcbor_state_t *state, const struct ota_profile *input);
static bool encode_command_result_ok(zcbor_state_t *state, const struct command_result_ok *input);
static bool encode_command_result_error(zcbor_state_t *state, const struct command_result_error *input);
static bool encode_repeated_config_begin_config_command_id(zcbor_state_t *state, const struct config_begin_config_command_id *input);
static bool encode_state_reading(zcbor_state_t *state, const struct state_reading *input);
static bool encode_state_readings(zcbor_state_t *state, const struct state_readings *input);
static bool encode_typed_value(zcbor_state_t *state, const struct typed_value_fields_r *input);
static bool encode_repeated_datastream_record_datastream_minimum(zcbor_state_t *state, const struct datastream_record_datastream_minimum *input);
static bool encode_repeated_datastream_record_datastream_maximum(zcbor_state_t *state, const struct datastream_record_datastream_maximum *input);
static bool encode_repeated_datastream_record_datastream_default(zcbor_state_t *state, const struct datastream_record_datastream_default *input);
static bool encode_pin_reference(zcbor_state_t *state, const struct pin_reference_r *input);
static bool encode_repeated_hardware_mapping_mapping_active_high(zcbor_state_t *state, const struct hardware_mapping_mapping_active_high *input);
static bool encode_repeated_hardware_mapping_mapping_pull(zcbor_state_t *state, const struct hardware_mapping_mapping_pull *input);
static bool encode_repeated_hardware_mapping_mapping_debounce_ms(zcbor_state_t *state, const struct hardware_mapping_mapping_debounce_ms *input);
static bool encode_repeated_hardware_mapping_mapping_sample_ms(zcbor_state_t *state, const struct hardware_mapping_mapping_sample_ms *input);
static bool encode_repeated_hardware_mapping_mapping_min_output_ms(zcbor_state_t *state, const struct hardware_mapping_mapping_min_output_ms *input);
static bool encode_hardware_mapping(zcbor_state_t *state, const struct hardware_mapping *input);
static bool encode_repeated_datastream_record_datastream_mapping(zcbor_state_t *state, const struct datastream_record_datastream_mapping *input);
static bool encode_datastream_record(zcbor_state_t *state, const struct datastream_record *input);
static bool encode_repeated_system_record_system_heartbeat_ms(zcbor_state_t *state, const struct system_record_system_heartbeat_ms *input);
static bool encode_system_pin_reference(zcbor_state_t *state, const struct system_pin_reference_r *input);
static bool encode_repeated_system_record_system_status_led_pin(zcbor_state_t *state, const struct system_record_system_status_led_pin *input);
static bool encode_repeated_system_record_system_status_led_active_low(zcbor_state_t *state, const struct system_record_system_status_led_active_low *input);
static bool encode_repeated_system_record_system_batch_flush_ms(zcbor_state_t *state, const struct system_record_system_batch_flush_ms *input);
static bool encode_repeated_system_record_system_factory_reset_pin(zcbor_state_t *state, const struct system_record_system_factory_reset_pin *input);
static bool encode_repeated_system_record_system_factory_reset_active_low(zcbor_state_t *state, const struct system_record_system_factory_reset_active_low *input);
static bool encode_repeated_system_record_system_factory_reset_profile(zcbor_state_t *state, const struct system_record_system_factory_reset_profile *input);
static bool encode_repeated_system_record_system_factory_reset_tap_count(zcbor_state_t *state, const struct system_record_system_factory_reset_tap_count *input);
static bool encode_repeated_system_record_system_factory_reset_hold_ms(zcbor_state_t *state, const struct system_record_system_factory_reset_hold_ms *input);
static bool encode_repeated_system_record_system_factory_reset_window_ms(zcbor_state_t *state, const struct system_record_system_factory_reset_window_ms *input);
static bool encode_repeated_system_record_system_factory_reset_debounce_ms(zcbor_state_t *state, const struct system_record_system_factory_reset_debounce_ms *input);
static bool encode_repeated_system_record_system_factory_reset_release_confirm(zcbor_state_t *state, const struct system_record_system_factory_reset_release_confirm *input);
static bool encode_system_record(zcbor_state_t *state, const struct system_record *input);
static bool encode_repeated_safety_record_safety_minimum(zcbor_state_t *state, const struct safety_record_safety_minimum *input);
static bool encode_repeated_safety_record_safety_maximum(zcbor_state_t *state, const struct safety_record_safety_maximum *input);
static bool encode_repeated_safety_record_safety_timeout_ms(zcbor_state_t *state, const struct safety_record_safety_timeout_ms *input);
static bool encode_repeated_safety_record_safety_guard_id(zcbor_state_t *state, const struct safety_record_safety_guard_id *input);
static bool encode_repeated_safety_record_safety_safe_value(zcbor_state_t *state, const struct safety_record_safety_safe_value *input);
static bool encode_repeated_safety_record_safety_guard_max_age_ms(zcbor_state_t *state, const struct safety_record_safety_guard_max_age_ms *input);
static bool encode_safety_record(zcbor_state_t *state, const struct safety_record *input);
static bool encode_repeated_parameter_record_parameter_value(zcbor_state_t *state, const struct parameter_record_parameter_value *input);
static bool encode_repeated_parameter_record_parameter_default(zcbor_state_t *state, const struct parameter_record_parameter_default *input);
static bool encode_parameter_record(zcbor_state_t *state, const struct parameter_record *input);
static bool encode_config_record_body(zcbor_state_t *state, const struct config_record_body_r *input);
static bool encode_error(zcbor_state_t *state, const struct zcbor_string *input);
static bool encode_config_end(zcbor_state_t *state, const struct config_end *input);
static bool encode_config_record(zcbor_state_t *state, const struct config_record *input);
static bool encode_config_begin(zcbor_state_t *state, const struct config_begin *input);
static bool encode_message_rejected(zcbor_state_t *state, const struct zcbor_string *input);
static bool encode_flow_control(zcbor_state_t *state, const struct flow_control *input);
static bool encode_time_response(zcbor_state_t *state, const struct time_response *input);
static bool encode_ota_desired(zcbor_state_t *state, const struct ota_desired *input);
static bool encode_config_desired(zcbor_state_t *state, const struct config_desired *input);
static bool encode_ingestion_ack(zcbor_state_t *state, const void *input);
static bool encode_command(zcbor_state_t *state, const struct command *input);
static bool encode_guardrail_report(zcbor_state_t *state, const struct guardrail_report *input);
static bool encode_log_event(zcbor_state_t *state, const struct log_event *input);
static bool encode_lifecycle_ready(zcbor_state_t *state, const void *input);
static bool encode_lifecycle_report(zcbor_state_t *state, const struct lifecycle_report *input);
static bool encode_config_ack(zcbor_state_t *state, const struct config_ack *input);
static bool encode_time_request(zcbor_state_t *state, const struct time_request *input);
static bool encode_ota_reported(zcbor_state_t *state, const struct ota_reported *input);
static bool encode_config_reported(zcbor_state_t *state, const struct config_reported *input);
static bool encode_command_result(zcbor_state_t *state, const struct command_result_r *input);
static bool encode_state(zcbor_state_t *state, const struct state *input);
static bool encode_heartbeat(zcbor_state_t *state, const struct heartbeat *input);
static bool encode_scheduling_declarations(zcbor_state_t *state, const struct scheduling_declarations *input);
static bool encode_scheduling_ready(zcbor_state_t *state, const void *input);
static bool encode_datastream_bound(zcbor_state_t *state, const struct datastream_bound *input);
static bool encode_datastream_bind(zcbor_state_t *state, const struct datastream_bind *input);
static bool encode_bootstrap_error(zcbor_state_t *state, const struct zcbor_string *input);
static bool encode_bootstrap_committed(zcbor_state_t *state, const struct bootstrap_committed *input);
static bool encode_bootstrap_auth(zcbor_state_t *state, const struct bootstrap_auth *input);
static bool encode_pong(zcbor_state_t *state, const void *input);
static bool encode_ping(zcbor_state_t *state, const void *input);
static bool encode_auth_error(zcbor_state_t *state, const struct zcbor_string *input);
static bool encode_auth_ok(zcbor_state_t *state, const struct auth_ok *input);
static bool encode_auth(zcbor_state_t *state, const struct auth *input);


static bool encode_repeated_ota_desired_ota_target(
		zcbor_state_t *state, const struct ota_desired_ota_target *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (5))))
	&& ((((*input).ota_desired_ota_target.len >= 1)
	&& ((*input).ota_desired_ota_target.len <= 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).ota_desired_ota_target)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_ota_desired_ota_release_id(
		zcbor_state_t *state, const struct ota_desired_ota_release_id *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (6))))
	&& ((((((*input).ota_desired_ota_release_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).ota_desired_ota_release_id)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_datastream_binding_keys_tstr1_48(
		zcbor_state_t *state, const struct zcbor_string *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((((*input).len >= 1)
	&& ((*input).len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input))))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_datastream_binding_keys(
		zcbor_state_t *state, const struct datastream_binding_keys *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 64) && ((zcbor_multi_encode_minmax(1, 64, &(*input).datastream_binding_keys_tstr1_48_count, (zcbor_encoder_t *)encode_repeated_datastream_binding_keys_tstr1_48, state, (*&(*input).datastream_binding_keys_tstr1_48), sizeof(struct zcbor_string))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 64))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_guardrail_report_guardrail_incident_l(
		zcbor_state_t *state, const struct guardrail_report_guardrail_incident_l *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((((((*input).guardrail_report_guardrail_incident_l_guardrail_incident >= 1)
	&& ((*input).guardrail_report_guardrail_incident_l_guardrail_incident <= 5)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).guardrail_report_guardrail_incident_l_guardrail_incident))))
	&& ((zcbor_uint64_encode(state, (&(*input).guardrail_report_guardrail_incident_l_guardrail_incident_id)))))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_typed_value_fields(
		zcbor_state_t *state, const struct typed_value_fields_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((((*input).typed_value_fields_choice == typed_value_fields_value_bool_type_l_c) ? ((((zcbor_uint64_put(state, (0))))
	&& ((zcbor_bool_encode(state, (&(*input).typed_value_fields_value_bool_type_l_value_bool))))))
	: (((*input).typed_value_fields_choice == typed_value_fields_value_int_type_l_c) ? ((((zcbor_uint64_put(state, (1))))
	&& ((zcbor_int64_encode(state, (&(*input).typed_value_fields_value_int_type_l_value_int))))))
	: (((*input).typed_value_fields_choice == typed_value_fields_value_f32_type_l_c) ? ((((zcbor_uint64_put(state, (2))))
	&& ((zcbor_float32_encode(state, (&(*input).typed_value_fields_value_f32_type_l_value_f32))))))
	: (((*input).typed_value_fields_choice == typed_value_fields_value_f64_type_l_c) ? ((((zcbor_uint64_put(state, (3))))
	&& ((zcbor_float64_encode(state, (&(*input).typed_value_fields_value_f64_type_l_value_f64))))))
	: (((*input).typed_value_fields_choice == typed_value_fields_value_text_type_l_c) ? ((((zcbor_uint64_put(state, (4))))
	&& (((((((*input).typed_value_fields_value_text_type_l_value_text.len >= 0)
	&& ((*input).typed_value_fields_value_text_type_l_value_text.len <= 96)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).typed_value_fields_value_text_type_l_value_text))))))
	: false)))))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_correlation_id(
		zcbor_state_t *state, const struct correlation_id_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((((*input).correlation_id_choice == correlation_id_empty_id_m_c) ? (((((((*input).correlation_id_empty_id_m.len == 0)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).correlation_id_empty_id_m))))
	: (((*input).correlation_id_choice == correlation_id_uuid_m_c) ? (((((((*input).correlation_id_uuid_m.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).correlation_id_uuid_m))))
	: false))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_command_guardrail_latch(
		zcbor_state_t *state, const uint64_t *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_uint64_encode(state, (&(*input))))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_capabilities_pin_references(
		zcbor_state_t *state, const struct capabilities_pin_references *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (8))))
	&& (zcbor_bool_encode(state, (&(*input).capabilities_pin_references)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_capabilities_offline_guardrail_slots(
		zcbor_state_t *state, const struct capabilities_offline_guardrail_slots *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (9))))
	&& ((((((*input).capabilities_offline_guardrail_slots <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_offline_guardrail_slots)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_capabilities(
		zcbor_state_t *state, const struct capabilities *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 10) && (((((zcbor_uint64_put(state, (0))))
	&& ((((((((*input).capabilities_datastream_slots <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_datastream_slots))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((((((*input).capabilities_input_slots <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_input_slots))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((((((*input).capabilities_output_slots <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_output_slots))))
	&& (((zcbor_uint64_put(state, (3))))
	&& ((((((((*input).capabilities_command_slots <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_command_slots))))
	&& (((zcbor_uint64_put(state, (4))))
	&& ((((((((*input).capabilities_schedule_slots <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_schedule_slots))))
	&& (((zcbor_uint64_put(state, (5))))
	&& ((((((((*input).capabilities_manifest_bytes <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_manifest_bytes))))
	&& (((zcbor_uint64_put(state, (6))))
	&& ((((((((*input).capabilities_history_bytes <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).capabilities_history_bytes))))
	&& (((zcbor_uint64_put(state, (7))))
	&& (zcbor_uint64_put(state, (512))))
	&& (!(*input).capabilities_pin_references_present || encode_repeated_capabilities_pin_references(state, (&(*input).capabilities_pin_references)))
	&& (!(*input).capabilities_offline_guardrail_slots_present || encode_repeated_capabilities_offline_guardrail_slots(state, (&(*input).capabilities_offline_guardrail_slots)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 10))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_datastream_bound_ids_compact_id_m(
		zcbor_state_t *state, const uint64_t *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((((((((*input) <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input))))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_datastream_bound_ids(
		zcbor_state_t *state, const struct datastream_bound_ids *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 64) && ((zcbor_multi_encode_minmax(1, 64, &(*input).datastream_bound_ids_compact_id_m_count, (zcbor_encoder_t *)encode_repeated_datastream_bound_ids_compact_id_m, state, (*&(*input).datastream_bound_ids_compact_id_m), sizeof(uint64_t))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 64))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_scheduling_stream(
		zcbor_state_t *state, const struct scheduling_stream *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && (((((((((((*input).scheduling_stream_scheduling_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).scheduling_stream_scheduling_id))))
	&& (((((*input).scheduling_stream_scheduling_mode <= 1)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).scheduling_stream_scheduling_mode))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_ota_profile_ota_boot_state(
		zcbor_state_t *state, const struct ota_profile_ota_boot_state *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (4))))
	&& ((((*input).ota_profile_ota_boot_state <= 2)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).ota_profile_ota_boot_state)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_ota_profile_ota_rollback_reason(
		zcbor_state_t *state, const struct ota_profile_ota_rollback_reason *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (5))))
	&& ((((*input).ota_profile_ota_rollback_reason.len >= 1)
	&& ((*input).ota_profile_ota_rollback_reason.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).ota_profile_ota_rollback_reason)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_ota_profile(
		zcbor_state_t *state, const struct ota_profile *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 6) && (((((zcbor_uint64_put(state, (0))))
	&& ((((((*input).ota_profile_ota_max_image_bytes <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).ota_profile_ota_max_image_bytes))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((*input).ota_profile_ota_strategy <= 2)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).ota_profile_ota_strategy))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((*input).ota_profile_ota_boot_layout_version.len >= 1)
	&& ((*input).ota_profile_ota_boot_layout_version.len <= 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).ota_profile_ota_boot_layout_version))))
	&& (((zcbor_uint64_put(state, (3))))
	&& (zcbor_bool_encode(state, (&(*input).ota_profile_ota_rollback_capable))))
	&& (!(*input).ota_profile_ota_boot_state_present || encode_repeated_ota_profile_ota_boot_state(state, (&(*input).ota_profile_ota_boot_state)))
	&& (!(*input).ota_profile_ota_rollback_reason_present || encode_repeated_ota_profile_ota_rollback_reason(state, (&(*input).ota_profile_ota_rollback_reason)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 6))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_command_result_ok(
		zcbor_state_t *state, const struct command_result_ok *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 8) && (((((((((((*input).command_result_ok_result_ok_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_ok_result_ok_generation))))
	&& (((((((*input).command_result_ok_result_ok_command_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).command_result_ok_result_ok_command_id))))
	&& (((((((((*input).command_result_ok_result_ok_compact_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_ok_result_ok_compact_id))))
	&& (((((*input).command_result_ok_result_ok_status <= 2)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_ok_result_ok_status))))
	&& ((encode_typed_value_fields(state, (&(*input).command_result_ok_typed_value_fields_m))))
	&& (((((((*input).command_result_ok_result_ok_version <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_ok_result_ok_version))))
	&& ((encode_correlation_id(state, (&(*input).command_result_ok_result_ok_correlation_id))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 8))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_command_result_error(
		zcbor_state_t *state, const struct command_result_error *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 8) && (((((((((((*input).command_result_error_result_error_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_error_result_error_generation))))
	&& (((((((*input).command_result_error_result_error_command_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).command_result_error_result_error_command_id))))
	&& (((((((((*input).command_result_error_result_error_compact_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_error_result_error_compact_id))))
	&& (((((*input).command_result_error_result_error_status >= 3)
	&& ((*input).command_result_error_result_error_status <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_error_result_error_status))))
	&& (((((((*input).command_result_error_result_error_code.len >= 1)
	&& ((*input).command_result_error_result_error_code.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).command_result_error_result_error_code))))
	&& (((((((*input).command_result_error_result_error_message.len >= 0)
	&& ((*input).command_result_error_result_error_message.len <= 192)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).command_result_error_result_error_message))))
	&& (((((((*input).command_result_error_result_error_version <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_result_error_result_error_version))))
	&& ((encode_correlation_id(state, (&(*input).command_result_error_result_error_correlation_id))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 8))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_config_begin_config_command_id(
		zcbor_state_t *state, const struct config_begin_config_command_id *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (8))))
	&& ((((((*input).config_begin_config_command_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_begin_config_command_id)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_state_reading(
		zcbor_state_t *state, const struct state_reading *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && (((((((((((*input).state_reading_reading_compact_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).state_reading_reading_compact_id))))
	&& ((encode_typed_value_fields(state, (&(*input).state_reading_typed_value_fields_m))))
	&& (((((((*input).state_reading_reading_revision <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).state_reading_reading_revision))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_state_readings(
		zcbor_state_t *state, const struct state_readings *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && ((zcbor_multi_encode_minmax(1, 4, &(*input).state_readings_state_reading_m_count, (zcbor_encoder_t *)encode_state_reading, state, (*&(*input).state_readings_state_reading_m), sizeof(struct state_reading))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_typed_value(
		zcbor_state_t *state, const struct typed_value_fields_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && ((((encode_typed_value_fields(state, (&(*input)))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_datastream_record_datastream_minimum(
		zcbor_state_t *state, const struct datastream_record_datastream_minimum *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (5))))
	&& (encode_typed_value(state, (&(*input).datastream_record_datastream_minimum)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_datastream_record_datastream_maximum(
		zcbor_state_t *state, const struct datastream_record_datastream_maximum *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (6))))
	&& (encode_typed_value(state, (&(*input).datastream_record_datastream_maximum)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_datastream_record_datastream_default(
		zcbor_state_t *state, const struct datastream_record_datastream_default *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (7))))
	&& (encode_typed_value(state, (&(*input).datastream_record_datastream_default)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_pin_reference(
		zcbor_state_t *state, const struct pin_reference_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((((*input).pin_reference_choice == pin_reference_uint16_m_c) ? (((((((*input).pin_reference_uint16_m <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).pin_reference_uint16_m))))
	: (((*input).pin_reference_choice == pin_reference_tstr1_64_c) ? (((((*input).pin_reference_tstr1_64.len >= 1)
	&& ((*input).pin_reference_tstr1_64.len <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).pin_reference_tstr1_64))))
	: false))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_hardware_mapping_mapping_active_high(
		zcbor_state_t *state, const struct hardware_mapping_mapping_active_high *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (2))))
	&& (zcbor_bool_encode(state, (&(*input).hardware_mapping_mapping_active_high)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_hardware_mapping_mapping_pull(
		zcbor_state_t *state, const struct hardware_mapping_mapping_pull *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (3))))
	&& ((((*input).hardware_mapping_mapping_pull <= 3)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).hardware_mapping_mapping_pull)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_hardware_mapping_mapping_debounce_ms(
		zcbor_state_t *state, const struct hardware_mapping_mapping_debounce_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (4))))
	&& ((((((*input).hardware_mapping_mapping_debounce_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).hardware_mapping_mapping_debounce_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_hardware_mapping_mapping_sample_ms(
		zcbor_state_t *state, const struct hardware_mapping_mapping_sample_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (5))))
	&& ((((((*input).hardware_mapping_mapping_sample_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).hardware_mapping_mapping_sample_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_hardware_mapping_mapping_min_output_ms(
		zcbor_state_t *state, const struct hardware_mapping_mapping_min_output_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (6))))
	&& ((((((*input).hardware_mapping_mapping_min_output_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).hardware_mapping_mapping_min_output_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_hardware_mapping(
		zcbor_state_t *state, const struct hardware_mapping *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 7) && (((((zcbor_uint64_put(state, (0))))
	&& ((((*input).hardware_mapping_mapping_kind <= 3)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).hardware_mapping_mapping_kind))))
	&& (((zcbor_uint64_put(state, (1))))
	&& (encode_pin_reference(state, (&(*input).hardware_mapping_mapping_pin))))
	&& (!(*input).hardware_mapping_mapping_active_high_present || encode_repeated_hardware_mapping_mapping_active_high(state, (&(*input).hardware_mapping_mapping_active_high)))
	&& (!(*input).hardware_mapping_mapping_pull_present || encode_repeated_hardware_mapping_mapping_pull(state, (&(*input).hardware_mapping_mapping_pull)))
	&& (!(*input).hardware_mapping_mapping_debounce_ms_present || encode_repeated_hardware_mapping_mapping_debounce_ms(state, (&(*input).hardware_mapping_mapping_debounce_ms)))
	&& (!(*input).hardware_mapping_mapping_sample_ms_present || encode_repeated_hardware_mapping_mapping_sample_ms(state, (&(*input).hardware_mapping_mapping_sample_ms)))
	&& (!(*input).hardware_mapping_mapping_min_output_ms_present || encode_repeated_hardware_mapping_mapping_min_output_ms(state, (&(*input).hardware_mapping_mapping_min_output_ms)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 7))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_datastream_record_datastream_mapping(
		zcbor_state_t *state, const struct datastream_record_datastream_mapping *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (8))))
	&& (encode_hardware_mapping(state, (&(*input).datastream_record_datastream_mapping)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_datastream_record(
		zcbor_state_t *state, const struct datastream_record *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 9) && (((((zcbor_uint64_put(state, (0))))
	&& (zcbor_uint64_put(state, (0))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((((((*input).datastream_record_datastream_compact_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).datastream_record_datastream_compact_id))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((((*input).datastream_record_datastream_uuid.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).datastream_record_datastream_uuid))))
	&& (((zcbor_uint64_put(state, (3))))
	&& ((((*input).datastream_record_datastream_key.len >= 1)
	&& ((*input).datastream_record_datastream_key.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).datastream_record_datastream_key))))
	&& (((zcbor_uint64_put(state, (4))))
	&& ((((*input).datastream_record_datastream_value_type <= 4)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).datastream_record_datastream_value_type))))
	&& (!(*input).datastream_record_datastream_minimum_present || encode_repeated_datastream_record_datastream_minimum(state, (&(*input).datastream_record_datastream_minimum)))
	&& (!(*input).datastream_record_datastream_maximum_present || encode_repeated_datastream_record_datastream_maximum(state, (&(*input).datastream_record_datastream_maximum)))
	&& (!(*input).datastream_record_datastream_default_present || encode_repeated_datastream_record_datastream_default(state, (&(*input).datastream_record_datastream_default)))
	&& (!(*input).datastream_record_datastream_mapping_present || encode_repeated_datastream_record_datastream_mapping(state, (&(*input).datastream_record_datastream_mapping)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 9))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_heartbeat_ms(
		zcbor_state_t *state, const struct system_record_system_heartbeat_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (1))))
	&& ((((((*input).system_record_system_heartbeat_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_heartbeat_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_system_pin_reference(
		zcbor_state_t *state, const struct system_pin_reference_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((((*input).system_pin_reference_choice == system_pin_reference_uint8_m_c) ? (((((((*input).system_pin_reference_uint8_m <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_pin_reference_uint8_m))))
	: (((*input).system_pin_reference_choice == system_pin_reference_tstr1_64_c) ? (((((*input).system_pin_reference_tstr1_64.len >= 1)
	&& ((*input).system_pin_reference_tstr1_64.len <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).system_pin_reference_tstr1_64))))
	: false))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_status_led_pin(
		zcbor_state_t *state, const struct system_record_system_status_led_pin *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (2))))
	&& (encode_system_pin_reference(state, (&(*input).system_record_system_status_led_pin)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_status_led_active_low(
		zcbor_state_t *state, const struct system_record_system_status_led_active_low *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (3))))
	&& (zcbor_bool_encode(state, (&(*input).system_record_system_status_led_active_low)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_batch_flush_ms(
		zcbor_state_t *state, const struct system_record_system_batch_flush_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (4))))
	&& ((((((*input).system_record_system_batch_flush_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_batch_flush_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_pin(
		zcbor_state_t *state, const struct system_record_system_factory_reset_pin *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (5))))
	&& (encode_system_pin_reference(state, (&(*input).system_record_system_factory_reset_pin)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_active_low(
		zcbor_state_t *state, const struct system_record_system_factory_reset_active_low *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (6))))
	&& (zcbor_bool_encode(state, (&(*input).system_record_system_factory_reset_active_low)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_profile(
		zcbor_state_t *state, const struct system_record_system_factory_reset_profile *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (7))))
	&& ((((*input).system_record_system_factory_reset_profile <= 1)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_factory_reset_profile)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_tap_count(
		zcbor_state_t *state, const struct system_record_system_factory_reset_tap_count *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (8))))
	&& ((((*input).system_record_system_factory_reset_tap_count >= 1)
	&& ((*input).system_record_system_factory_reset_tap_count <= 8)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_factory_reset_tap_count)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_hold_ms(
		zcbor_state_t *state, const struct system_record_system_factory_reset_hold_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (9))))
	&& ((((((*input).system_record_system_factory_reset_hold_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_factory_reset_hold_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_window_ms(
		zcbor_state_t *state, const struct system_record_system_factory_reset_window_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (10))))
	&& ((((((*input).system_record_system_factory_reset_window_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_factory_reset_window_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_debounce_ms(
		zcbor_state_t *state, const struct system_record_system_factory_reset_debounce_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (11))))
	&& ((((((*input).system_record_system_factory_reset_debounce_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).system_record_system_factory_reset_debounce_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_system_record_system_factory_reset_release_confirm(
		zcbor_state_t *state, const struct system_record_system_factory_reset_release_confirm *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (12))))
	&& (zcbor_bool_encode(state, (&(*input).system_record_system_factory_reset_release_confirm)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_system_record(
		zcbor_state_t *state, const struct system_record *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 13) && (((((zcbor_uint64_put(state, (0))))
	&& (zcbor_uint64_put(state, (1))))
	&& (!(*input).system_record_system_heartbeat_ms_present || encode_repeated_system_record_system_heartbeat_ms(state, (&(*input).system_record_system_heartbeat_ms)))
	&& (!(*input).system_record_system_status_led_pin_present || encode_repeated_system_record_system_status_led_pin(state, (&(*input).system_record_system_status_led_pin)))
	&& (!(*input).system_record_system_status_led_active_low_present || encode_repeated_system_record_system_status_led_active_low(state, (&(*input).system_record_system_status_led_active_low)))
	&& (!(*input).system_record_system_batch_flush_ms_present || encode_repeated_system_record_system_batch_flush_ms(state, (&(*input).system_record_system_batch_flush_ms)))
	&& (!(*input).system_record_system_factory_reset_pin_present || encode_repeated_system_record_system_factory_reset_pin(state, (&(*input).system_record_system_factory_reset_pin)))
	&& (!(*input).system_record_system_factory_reset_active_low_present || encode_repeated_system_record_system_factory_reset_active_low(state, (&(*input).system_record_system_factory_reset_active_low)))
	&& (!(*input).system_record_system_factory_reset_profile_present || encode_repeated_system_record_system_factory_reset_profile(state, (&(*input).system_record_system_factory_reset_profile)))
	&& (!(*input).system_record_system_factory_reset_tap_count_present || encode_repeated_system_record_system_factory_reset_tap_count(state, (&(*input).system_record_system_factory_reset_tap_count)))
	&& (!(*input).system_record_system_factory_reset_hold_ms_present || encode_repeated_system_record_system_factory_reset_hold_ms(state, (&(*input).system_record_system_factory_reset_hold_ms)))
	&& (!(*input).system_record_system_factory_reset_window_ms_present || encode_repeated_system_record_system_factory_reset_window_ms(state, (&(*input).system_record_system_factory_reset_window_ms)))
	&& (!(*input).system_record_system_factory_reset_debounce_ms_present || encode_repeated_system_record_system_factory_reset_debounce_ms(state, (&(*input).system_record_system_factory_reset_debounce_ms)))
	&& (!(*input).system_record_system_factory_reset_release_confirm_present || encode_repeated_system_record_system_factory_reset_release_confirm(state, (&(*input).system_record_system_factory_reset_release_confirm)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 13))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_safety_record_safety_minimum(
		zcbor_state_t *state, const struct safety_record_safety_minimum *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (3))))
	&& (encode_typed_value(state, (&(*input).safety_record_safety_minimum)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_safety_record_safety_maximum(
		zcbor_state_t *state, const struct safety_record_safety_maximum *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (4))))
	&& (encode_typed_value(state, (&(*input).safety_record_safety_maximum)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_safety_record_safety_timeout_ms(
		zcbor_state_t *state, const struct safety_record_safety_timeout_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (5))))
	&& ((((((*input).safety_record_safety_timeout_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).safety_record_safety_timeout_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_safety_record_safety_guard_id(
		zcbor_state_t *state, const struct safety_record_safety_guard_id *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (6))))
	&& ((((((((*input).safety_record_safety_guard_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).safety_record_safety_guard_id)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_safety_record_safety_safe_value(
		zcbor_state_t *state, const struct safety_record_safety_safe_value *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (7))))
	&& (encode_typed_value(state, (&(*input).safety_record_safety_safe_value)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_safety_record_safety_guard_max_age_ms(
		zcbor_state_t *state, const struct safety_record_safety_guard_max_age_ms *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (8))))
	&& ((((((*input).safety_record_safety_guard_max_age_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).safety_record_safety_guard_max_age_ms)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_safety_record(
		zcbor_state_t *state, const struct safety_record *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 9) && (((((zcbor_uint64_put(state, (0))))
	&& (zcbor_uint64_put(state, (3))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((((((*input).safety_record_safety_compact_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).safety_record_safety_compact_id))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((*input).safety_record_safety_policy <= 9)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).safety_record_safety_policy))))
	&& (!(*input).safety_record_safety_minimum_present || encode_repeated_safety_record_safety_minimum(state, (&(*input).safety_record_safety_minimum)))
	&& (!(*input).safety_record_safety_maximum_present || encode_repeated_safety_record_safety_maximum(state, (&(*input).safety_record_safety_maximum)))
	&& (!(*input).safety_record_safety_timeout_ms_present || encode_repeated_safety_record_safety_timeout_ms(state, (&(*input).safety_record_safety_timeout_ms)))
	&& (!(*input).safety_record_safety_guard_id_present || encode_repeated_safety_record_safety_guard_id(state, (&(*input).safety_record_safety_guard_id)))
	&& (!(*input).safety_record_safety_safe_value_present || encode_repeated_safety_record_safety_safe_value(state, (&(*input).safety_record_safety_safe_value)))
	&& (!(*input).safety_record_safety_guard_max_age_ms_present || encode_repeated_safety_record_safety_guard_max_age_ms(state, (&(*input).safety_record_safety_guard_max_age_ms)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 9))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_parameter_record_parameter_value(
		zcbor_state_t *state, const struct parameter_record_parameter_value *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (3))))
	&& (encode_typed_value(state, (&(*input).parameter_record_parameter_value)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_repeated_parameter_record_parameter_default(
		zcbor_state_t *state, const struct parameter_record_parameter_default *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = ((((zcbor_uint64_put(state, (4))))
	&& (encode_typed_value(state, (&(*input).parameter_record_parameter_default)))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_parameter_record(
		zcbor_state_t *state, const struct parameter_record *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 5) && (((((zcbor_uint64_put(state, (0))))
	&& (zcbor_uint64_put(state, (5))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((*input).parameter_record_parameter_key.len >= 1)
	&& ((*input).parameter_record_parameter_key.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).parameter_record_parameter_key))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((*input).parameter_record_parameter_value_type <= 4)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).parameter_record_parameter_value_type))))
	&& (!(*input).parameter_record_parameter_value_present || encode_repeated_parameter_record_parameter_value(state, (&(*input).parameter_record_parameter_value)))
	&& (!(*input).parameter_record_parameter_default_present || encode_repeated_parameter_record_parameter_default(state, (&(*input).parameter_record_parameter_default)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 5))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_record_body(
		zcbor_state_t *state, const struct config_record_body_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((((*input).config_record_body_choice == config_record_body_datastream_record_m_c) ? ((encode_datastream_record(state, (&(*input).config_record_body_datastream_record_m))))
	: (((*input).config_record_body_choice == config_record_body_system_record_m_c) ? ((encode_system_record(state, (&(*input).config_record_body_system_record_m))))
	: (((*input).config_record_body_choice == config_record_body_safety_record_m_c) ? ((encode_safety_record(state, (&(*input).config_record_body_safety_record_m))))
	: (((*input).config_record_body_choice == config_record_body_parameter_record_m_c) ? ((encode_parameter_record(state, (&(*input).config_record_body_parameter_record_m))))
	: false))))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_error(
		zcbor_state_t *state, const struct zcbor_string *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 1) && (((((((((*input).len >= 1)
	&& ((*input).len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input)))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_end(
		zcbor_state_t *state, const struct config_end *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 3) && (((((((((((*input).config_end_end_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_end_end_generation))))
	&& (((((((*input).config_end_end_record_count <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_end_end_record_count))))
	&& (((((((*input).config_end_end_checksum.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_end_end_checksum))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_record(
		zcbor_state_t *state, const struct config_record *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 3) && (((((zcbor_uint64_put(state, (0))))
	&& ((((((((*input).config_record_record_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_record_record_generation))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((((((*input).config_record_record_sequence <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_record_record_sequence))))
	&& (((zcbor_uint64_put(state, (2))))
	&& (encode_config_record_body(state, (&(*input).config_record_record_body))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 3))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_begin(
		zcbor_state_t *state, const struct config_begin *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 9) && (((((zcbor_uint64_put(state, (0))))
	&& ((((((((*input).config_begin_config_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_begin_config_generation))))
	&& (((zcbor_uint64_put(state, (1))))
	&& (zcbor_uint64_put(state, (1))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((((*input).config_begin_config_checksum.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_begin_config_checksum))))
	&& (((zcbor_uint64_put(state, (3))))
	&& ((((((*input).config_begin_config_record_count <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_begin_config_record_count))))
	&& (((zcbor_uint64_put(state, (4))))
	&& (encode_capabilities(state, (&(*input).config_begin_negotiated_limits))))
	&& (((zcbor_uint64_put(state, (5))))
	&& ((((((*input).config_begin_config_device_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_begin_config_device_id))))
	&& (((zcbor_uint64_put(state, (6))))
	&& ((((((*input).config_begin_config_template_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_begin_config_template_id))))
	&& (((zcbor_uint64_put(state, (7))))
	&& ((((((*input).config_begin_config_link_url.len >= 1)
	&& ((*input).config_begin_config_link_url.len <= 192)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).config_begin_config_link_url))))
	&& (!(*input).config_begin_config_command_id_present || encode_repeated_config_begin_config_command_id(state, (&(*input).config_begin_config_command_id)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 9))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_message_rejected(
		zcbor_state_t *state, const struct zcbor_string *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 1) && (((((((((*input).len >= 1)
	&& ((*input).len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input)))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_flow_control(
		zcbor_state_t *state, const struct flow_control *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 3) && (((((((((*input).flow_control_flow_retry_after_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).flow_control_flow_retry_after_ms))))
	&& (((((((*input).flow_control_flow_reason.len >= 1)
	&& ((*input).flow_control_flow_reason.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).flow_control_flow_reason))))
	&& (((((((*input).flow_control_flow_capacity <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).flow_control_flow_capacity))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_time_response(
		zcbor_state_t *state, const struct time_response *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && ((((zcbor_uint64_encode(state, (&(*input).time_response_request_id))))
	&& ((zcbor_uint64_encode(state, (&(*input).time_response_server_utc_ms))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_ota_desired(
		zcbor_state_t *state, const struct ota_desired *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 7) && (((((zcbor_uint64_put(state, (0))))
	&& ((((((*input).ota_desired_ota_install_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).ota_desired_ota_install_id))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((*input).ota_desired_ota_version.len >= 1)
	&& ((*input).ota_desired_ota_version.len <= 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).ota_desired_ota_version))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((((*input).ota_desired_ota_url.len >= 1)
	&& ((*input).ota_desired_ota_url.len <= 256)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).ota_desired_ota_url))))
	&& (((zcbor_uint64_put(state, (3))))
	&& ((((((*input).ota_desired_ota_checksum.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).ota_desired_ota_checksum))))
	&& (((zcbor_uint64_put(state, (4))))
	&& ((((((*input).ota_desired_ota_size <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).ota_desired_ota_size))))
	&& (!(*input).ota_desired_ota_target_present || encode_repeated_ota_desired_ota_target(state, (&(*input).ota_desired_ota_target)))
	&& (!(*input).ota_desired_ota_release_id_present || encode_repeated_ota_desired_ota_release_id(state, (&(*input).ota_desired_ota_release_id)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 7))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_desired(
		zcbor_state_t *state, const struct config_desired *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 3) && (((((((((((*input).config_desired_desired_config_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_desired_desired_config_generation))))
	&& (((((((*input).config_desired_desired_config_checksum.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_desired_desired_config_checksum))))
	&& (((((((*input).config_desired_desired_config_record_count <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_desired_desired_config_record_count))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_ingestion_ack(
		zcbor_state_t *state, const void *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 0) && zcbor_list_end_encode(state, 0))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_command(
		zcbor_state_t *state, const struct command *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 9) && (((((((((((*input).command_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_generation))))
	&& (((((((*input).command_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).command_id))))
	&& (((((((((*input).command_compact_id <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_compact_id))))
	&& ((encode_typed_value_fields(state, (&(*input).command_typed_value_fields_m))))
	&& (((((((*input).command_desired_version <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).command_desired_version))))
	&& ((zcbor_uint64_encode(state, (&(*input).command_expires_at_utc_ms))))
	&& ((encode_correlation_id(state, (&(*input).command_correlation_id))))
	&& (!(*input).command_guardrail_latch_present || encode_repeated_command_guardrail_latch(state, (&(*input).command_guardrail_latch)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 9))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_guardrail_report(
		zcbor_state_t *state, const struct guardrail_report *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 6) && (((((((((((*input).guardrail_report_guardrail_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).guardrail_report_guardrail_generation))))
	&& (((((((((*input).guardrail_report_guardrail_stream <= UINT16_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).guardrail_report_guardrail_stream))))
	&& (((((*input).guardrail_report_guardrail_status >= 1)
	&& ((*input).guardrail_report_guardrail_status <= 7)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).guardrail_report_guardrail_status))))
	&& ((zcbor_uint64_encode(state, (&(*input).guardrail_report_guardrail_latch))))
	&& (!(*input).guardrail_report_guardrail_incident_l_present || encode_repeated_guardrail_report_guardrail_incident_l(state, (&(*input).guardrail_report_guardrail_incident_l)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 6))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_log_event(
		zcbor_state_t *state, const struct log_event *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && ((((zcbor_uint64_encode(state, (&(*input).log_event_event_boot_id))))
	&& (((((((*input).log_event_event_sequence <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).log_event_event_sequence))))
	&& (((((*input).log_event_event_code.len >= 1)
	&& ((*input).log_event_event_code.len <= 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).log_event_event_code))))
	&& (((((((*input).log_event_event_message.len >= 0)
	&& ((*input).log_event_event_message.len <= 96)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).log_event_event_message))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_lifecycle_ready(
		zcbor_state_t *state, const void *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 0) && zcbor_list_end_encode(state, 0))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_lifecycle_report(
		zcbor_state_t *state, const struct lifecycle_report *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 7) && ((((zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_boot_id))))
	&& (((((((*input).lifecycle_report_log_sequence <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_sequence))))
	&& ((zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_uptime_ms))))
	&& (((((*input).lifecycle_report_log_code <= 10)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_code))))
	&& (((((((*input).lifecycle_report_log_detail <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_detail))))
	&& (((((((((*input).lifecycle_report_log_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_generation))))
	&& (((((((*input).lifecycle_report_log_dropped <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).lifecycle_report_log_dropped))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 7))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_ack(
		zcbor_state_t *state, const struct config_ack *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && (((((((((((*input).config_ack_ack_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_ack_ack_generation))))
	&& (((((((((*input).config_ack_ack_sequence <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_ack_ack_sequence))))
	&& (((((*input).config_ack_ack_status <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_ack_ack_status))))
	&& (((((((*input).config_ack_ack_reason.len >= 1)
	&& ((*input).config_ack_ack_reason.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).config_ack_ack_reason))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_time_request(
		zcbor_state_t *state, const struct time_request *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && ((((zcbor_uint64_encode(state, (&(*input).time_request_id))))
	&& ((zcbor_uint64_encode(state, (&(*input).time_request_monotonic_ms))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_ota_reported(
		zcbor_state_t *state, const struct ota_reported *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && (((((((((*input).ota_reported_reported_ota_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).ota_reported_reported_ota_id))))
	&& (((((*input).ota_reported_reported_ota_status <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).ota_reported_reported_ota_status))))
	&& (((((((*input).ota_reported_reported_ota_error.len >= 1)
	&& ((*input).ota_reported_reported_ota_error.len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).ota_reported_reported_ota_error))))
	&& (((((((*input).ota_reported_reported_ota_progress <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).ota_reported_reported_ota_progress))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_config_reported(
		zcbor_state_t *state, const struct config_reported *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 3) && (((((((((((*input).config_reported_reported_config_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_reported_reported_config_generation))))
	&& (((((*input).config_reported_reported_config_status <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).config_reported_reported_config_status))))
	&& (((((((*input).config_reported_reported_config_checksum.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).config_reported_reported_config_checksum))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_command_result(
		zcbor_state_t *state, const struct command_result_r *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((((*input).command_result_choice == command_result_ok_m_c) ? ((encode_command_result_ok(state, (&(*input).command_result_ok_m))))
	: (((*input).command_result_choice == command_result_error_m_c) ? ((encode_command_result_error(state, (&(*input).command_result_error_m))))
	: false))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_state(
		zcbor_state_t *state, const struct state *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && (((((((((((*input).state_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).state_generation))))
	&& ((encode_state_readings(state, (&(*input).state_values))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_heartbeat(
		zcbor_state_t *state, const struct heartbeat *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 9) && (((((((((((*input).heartbeat_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).heartbeat_generation))))
	&& ((zcbor_uint64_encode(state, (&(*input).heartbeat_uptime_ms))))
	&& (((((((*input).heartbeat_status <= UINT8_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).heartbeat_status))))
	&& (((((*input).heartbeat_firmware_version.len >= 1)
	&& ((*input).heartbeat_firmware_version.len <= 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).heartbeat_firmware_version))))
	&& (((((*input).heartbeat_firmware_target.len >= 1)
	&& ((*input).heartbeat_firmware_target.len <= 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).heartbeat_firmware_target))))
	&& ((((*input).heartbeat_running_release_id_choice == heartbeat_running_release_id_empty_id_m_c) ? (((((((*input).heartbeat_running_release_id_empty_id_m.len == 0)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).heartbeat_running_release_id_empty_id_m))))
	: (((*input).heartbeat_running_release_id_choice == heartbeat_running_release_id_uuid_m_c) ? (((((((*input).heartbeat_running_release_id_uuid_m.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).heartbeat_running_release_id_uuid_m))))
	: false)))
	&& ((((*input).heartbeat_last_install_id_choice == heartbeat_last_install_id_empty_id_m_c) ? (((((((*input).heartbeat_last_install_id_empty_id_m.len == 0)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).heartbeat_last_install_id_empty_id_m))))
	: (((*input).heartbeat_last_install_id_choice == heartbeat_last_install_id_uuid_m_c) ? (((((((*input).heartbeat_last_install_id_uuid_m.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).heartbeat_last_install_id_uuid_m))))
	: false)))
	&& ((encode_ota_profile(state, (&(*input).heartbeat_ota_profile))))
	&& (!(*input).heartbeat_capabilities_present || encode_capabilities(state, (&(*input).heartbeat_capabilities)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 9))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_scheduling_declarations(
		zcbor_state_t *state, const struct scheduling_declarations *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && (((((((((((*input).scheduling_declarations_scheduling_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).scheduling_declarations_scheduling_generation))))
	&& (((((*input).scheduling_declarations_scheduling_offset <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).scheduling_declarations_scheduling_offset))))
	&& (((((*input).scheduling_declarations_scheduling_total <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).scheduling_declarations_scheduling_total))))
	&& ((zcbor_list_start_encode(state, 16) && ((zcbor_multi_encode_minmax(0, 16, &(*input).scheduling_declarations_scheduling_streams_scheduling_stream_m_count, (zcbor_encoder_t *)encode_scheduling_stream, state, (*&(*input).scheduling_declarations_scheduling_streams_scheduling_stream_m), sizeof(struct scheduling_stream))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 16)))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_scheduling_ready(
		zcbor_state_t *state, const void *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 0) && zcbor_list_end_encode(state, 0))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_datastream_bound(
		zcbor_state_t *state, const struct datastream_bound *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && (((((((((((*input).datastream_bound_bound_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).datastream_bound_bound_generation))))
	&& ((encode_datastream_bound_ids(state, (&(*input).datastream_bound_bound_ids))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_datastream_bind(
		zcbor_state_t *state, const struct datastream_bind *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && (((((((((((*input).datastream_bind_binding_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).datastream_bind_binding_generation))))
	&& ((encode_datastream_binding_keys(state, (&(*input).datastream_bind_binding_keys))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_bootstrap_error(
		zcbor_state_t *state, const struct zcbor_string *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 1) && (((((((((*input).len >= 1)
	&& ((*input).len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input)))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_bootstrap_committed(
		zcbor_state_t *state, const struct bootstrap_committed *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 3) && (((((((((*input).bootstrap_committed_committed_device_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).bootstrap_committed_committed_device_id))))
	&& (((((((((*input).bootstrap_committed_committed_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).bootstrap_committed_committed_generation))))
	&& ((zcbor_uint64_encode(state, (&(*input).bootstrap_committed_committed_server_utc_ms))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 3))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_bootstrap_auth(
		zcbor_state_t *state, const struct bootstrap_auth *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_map_start_encode(state, 5) && (((((zcbor_uint64_put(state, (0))))
	&& ((((((*input).bootstrap_auth_bootstrap_token.len >= 32)
	&& ((*input).bootstrap_auth_bootstrap_token.len <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).bootstrap_auth_bootstrap_token))))
	&& (((zcbor_uint64_put(state, (1))))
	&& ((((((*input).bootstrap_auth_bootstrap_secret.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).bootstrap_auth_bootstrap_secret))))
	&& (((zcbor_uint64_put(state, (2))))
	&& ((((*input).bootstrap_auth_hardware_id.len >= 1)
	&& ((*input).bootstrap_auth_hardware_id.len <= 96)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).bootstrap_auth_hardware_id))))
	&& (((zcbor_uint64_put(state, (3))))
	&& ((((*input).bootstrap_auth_firmware_target.len >= 1)
	&& ((*input).bootstrap_auth_firmware_target.len <= 64)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input).bootstrap_auth_firmware_target))))
	&& (((zcbor_uint64_put(state, (4))))
	&& (encode_capabilities(state, (&(*input).bootstrap_auth_bootstrap_capabilities))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_map_end_encode(state, 5))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_pong(
		zcbor_state_t *state, const void *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 0) && zcbor_list_end_encode(state, 0))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_ping(
		zcbor_state_t *state, const void *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 0) && zcbor_list_end_encode(state, 0))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_auth_error(
		zcbor_state_t *state, const struct zcbor_string *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 1) && (((((((((*input).len >= 1)
	&& ((*input).len <= 48)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_tstr_encode(state, (&(*input)))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 1))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_auth_ok(
		zcbor_state_t *state, const struct auth_ok *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 4) && ((((zcbor_uint64_encode(state, (&(*input).auth_ok_auth_server_utc_ms))))
	&& (((((((*input).auth_ok_auth_heartbeat_ms <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).auth_ok_auth_heartbeat_ms))))
	&& ((zcbor_uint64_put(state, (512))))
	&& (((((((*input).auth_ok_auth_connection_generation <= UINT32_MAX)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_uint64_encode(state, (&(*input).auth_ok_auth_connection_generation))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 4))));

	log_result(state, res, __func__);
	return res;
}

static bool encode_auth(
		zcbor_state_t *state, const struct auth *input)
{
	zcbor_log("%s\r\n", __func__);

	bool res = (((zcbor_list_start_encode(state, 2) && (((((((((*input).auth_device_id.len == 16)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).auth_device_id))))
	&& (((((((*input).auth_secret.len == 32)) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))) || (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false))
	&& (zcbor_bstr_encode(state, (&(*input).auth_secret))))) || (zcbor_list_map_end_force_encode(state), false)) && zcbor_list_end_encode(state, 2))));

	log_result(state, res, __func__);
	return res;
}



int cbor_encode_auth(
		uint8_t *payload, size_t payload_len,
		const struct auth *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_auth, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_auth_ok(
		uint8_t *payload, size_t payload_len,
		const struct auth_ok *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_auth_ok, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_auth_error(
		uint8_t *payload, size_t payload_len,
		const struct zcbor_string *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_auth_error, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_ping(
		uint8_t *payload, size_t payload_len,
		const void *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_ping, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_pong(
		uint8_t *payload, size_t payload_len,
		const void *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_pong, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_bootstrap_auth(
		uint8_t *payload, size_t payload_len,
		const struct bootstrap_auth *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_bootstrap_auth, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_bootstrap_committed(
		uint8_t *payload, size_t payload_len,
		const struct bootstrap_committed *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_bootstrap_committed, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_bootstrap_error(
		uint8_t *payload, size_t payload_len,
		const struct zcbor_string *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_bootstrap_error, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_datastream_bind(
		uint8_t *payload, size_t payload_len,
		const struct datastream_bind *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_datastream_bind, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_datastream_bound(
		uint8_t *payload, size_t payload_len,
		const struct datastream_bound *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_datastream_bound, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_scheduling_ready(
		uint8_t *payload, size_t payload_len,
		const void *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_scheduling_ready, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_scheduling_declarations(
		uint8_t *payload, size_t payload_len,
		const struct scheduling_declarations *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[5];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_scheduling_declarations, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_heartbeat(
		uint8_t *payload, size_t payload_len,
		const struct heartbeat *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_heartbeat, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_state(
		uint8_t *payload, size_t payload_len,
		const struct state *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[6];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_state, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_command_result(
		uint8_t *payload, size_t payload_len,
		const struct command_result_r *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[5];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_command_result, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_config_reported(
		uint8_t *payload, size_t payload_len,
		const struct config_reported *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_config_reported, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_ota_reported(
		uint8_t *payload, size_t payload_len,
		const struct ota_reported *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_ota_reported, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_time_request(
		uint8_t *payload, size_t payload_len,
		const struct time_request *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_time_request, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_config_ack(
		uint8_t *payload, size_t payload_len,
		const struct config_ack *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_config_ack, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_lifecycle_report(
		uint8_t *payload, size_t payload_len,
		const struct lifecycle_report *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_lifecycle_report, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_lifecycle_ready(
		uint8_t *payload, size_t payload_len,
		const void *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_lifecycle_ready, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_log_event(
		uint8_t *payload, size_t payload_len,
		const struct log_event *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_log_event, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_guardrail_report(
		uint8_t *payload, size_t payload_len,
		const struct guardrail_report *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_guardrail_report, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_command(
		uint8_t *payload, size_t payload_len,
		const struct command *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_command, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_ingestion_ack(
		uint8_t *payload, size_t payload_len,
		const void *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_ingestion_ack, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_config_desired(
		uint8_t *payload, size_t payload_len,
		const struct config_desired *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_config_desired, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_ota_desired(
		uint8_t *payload, size_t payload_len,
		const struct ota_desired *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_ota_desired, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_time_response(
		uint8_t *payload, size_t payload_len,
		const struct time_response *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_time_response, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_flow_control(
		uint8_t *payload, size_t payload_len,
		const struct flow_control *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_flow_control, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_message_rejected(
		uint8_t *payload, size_t payload_len,
		const struct zcbor_string *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_message_rejected, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_config_begin(
		uint8_t *payload, size_t payload_len,
		const struct config_begin *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[4];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_config_begin, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_config_record(
		uint8_t *payload, size_t payload_len,
		const struct config_record *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[7];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_config_record, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_config_end(
		uint8_t *payload, size_t payload_len,
		const struct config_end *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_config_end, sizeof(states) / sizeof(zcbor_state_t), 1);
}


int cbor_encode_error(
		uint8_t *payload, size_t payload_len,
		const struct zcbor_string *input,
		size_t *payload_len_out)
{
	zcbor_state_t states[3];

	return zcbor_entry_function(payload, payload_len, (void *)input, payload_len_out, states,
		(zcbor_decoder_t *)encode_error, sizeof(states) / sizeof(zcbor_state_t), 1);
}
