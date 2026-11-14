alias FlovaEngine.{Repo, EndUserAccess}
alias FlovaEngine.EndUserAccess.EndUser
alias FlovaEngine.Runtime.DeviceLatestValue

[action, config_path, identity_path] = System.argv()
Logger.configure(level: :warning)
unless Mix.env() == :dev and Application.get_env(:flova_engine, :local_dev),
  do: raise("Board checks require local development")
endpoint = Application.fetch_env!(:flova_engine, FlovaEngineWeb.Endpoint)
Application.put_env(:flova_engine, FlovaEngineWeb.Endpoint, Keyword.put(endpoint, :server, false))
for key <- [:start_oban?, :start_outbox_worker?, :start_ingestion_recovery?, :start_entitlement_sync?, :start_presence_monitor?],
  do: Application.put_env(:flova_engine, key, false)
{:ok, _} = Application.ensure_all_started(:flova_engine)

config = config_path |> File.read!() |> Jason.decode!()
identity = identity_path |> File.read!() |> Jason.decode!()
unless config["development"] == true and identity["development"] == true,
  do: raise("Expected dedicated development fixture and identity")

user = Repo.get_by!(EndUser, auth_subject: identity["userId"])
{:ok, device} = EndUserAccess.get_authorized_device(user, config["device_id"])
unless device.organization_id == config["organization_id"], do: raise("Fixture organization mismatch")

case action do
  "refresh" ->
    {:ok, result} = EndUserAccess.regenerate_provisioning_session(user, device.id)
    updated = Map.put(config, "provision_token", result.provisioning.provisionToken)
    File.chmod!(config_path, 0o600)
    File.write!(config_path, Jason.encode!(updated, pretty: true))
    IO.puts("Refreshed dedicated development provisioning token")

  "command" ->
    latest = fn -> Repo.get_by(DeviceLatestValue, device_id: device.id, datastream_key: "power") end
    before = latest.()
    version = if before, do: before.desired_version, else: 0
    value = !(before && before.reported_value_json == %{"value" => true})
    tokens = Application.get_env(:flova_engine, :internal_api_tokens, %{})
    token = Map.get(tokens, "flova-console") || Application.get_env(:flova_engine, :internal_api_token)
    headers = [
      {"x-flova-internal-source", "flova-console"},
      {"x-flova-organization-id", device.organization_id},
      {"x-flova-user-id", identity["userId"]},
      {"x-flova-permissions", "devices.command"}
    ] ++ if(token in [nil, ""], do: [], else: [{"authorization", "Bearer " <> token}])
    response = Req.post!("http://127.0.0.1:#{config["engine_port"]}/api/console/devices/#{device.id}/commands",
      headers: headers, json: %{datastream_key: "power", value: value},
      retry: false, receive_timeout: 5_000)
    unless response.status == 202, do: raise("Development command rejected (HTTP #{response.status})")
    deadline = System.monotonic_time(:millisecond) + 15_000
    wait = fn wait ->
      row = latest.()
      cond do
        row && row.desired_version > version && row.acknowledged_version == row.desired_version &&
            row.desired_value_json == %{"value" => value} && row.reported_value_json == row.desired_value_json ->
          IO.puts("PASS fresh server command applied and acknowledged")
        System.monotonic_time(:millisecond) >= deadline ->
          raise("Timed out waiting for fresh server command application/ACK")
        true ->
          receive do
          after
            100 -> wait.(wait)
          end
      end
    end
    wait.(wait)
end
