"""Temporary, verified TLS ingress to an existing loopback development MQTT broker."""

import os
import socket
import subprocess
import time


def make_certificates(output, address, run):
    run("test-ca", ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                    "-keyout", "ca.key", "-out", "ca.crt", "-days", "2", "-subj",
                    "/CN=Flova development board test CA", "-addext",
                    "basicConstraints=critical,CA:TRUE"], cwd=output)
    run("test-csr", ["openssl", "req", "-newkey", "rsa:2048", "-nodes", "-keyout",
                     "server.key", "-out", "server.csr", "-subj", f"/CN={address}"], cwd=output)
    # The DNS SAN also supports the installed ESP32 mbedTLS hostname verifier.
    (output / "server.ext").write_text(
        f"subjectAltName=DNS:{address},IP:{address}\n"
        "extendedKeyUsage=serverAuth\nbasicConstraints=CA:FALSE\n")
    run("test-certificate", ["openssl", "x509", "-req", "-in", "server.csr", "-CA",
                             "ca.crt", "-CAkey", "ca.key", "-CAcreateserial", "-out",
                             "server.crt", "-days", "2", "-extfile", "server.ext"], cwd=output)


class LocalIngress:
    """Use the same record-bounded TLS implementation as Flova's device edge."""

    def __init__(self, output, config):
        self.output, self.config = output, config
        self.name = f"flova-board-ingress-{os.getpid()}"
        self.process = None
        self.log = None

    def __enter__(self):
        pem = self.output / "server.pem"
        pem.write_text((self.output / "server.crt").read_text() +
                       (self.output / "server.key").read_text())
        (self.output / "haproxy.cfg").write_text(f"""global
  tune.ssl.maxrecord 2048
defaults
  mode tcp
  timeout connect 5s
  timeout client 120s
  timeout server 120s
  timeout tunnel 1h
frontend device_tls
  bind {self.config['lan_address']}:{self.config['tls_port']} ssl crt /test/server.pem ssl-min-ver TLSv1.2
  default_backend mqtt
backend mqtt
  server mqtt 127.0.0.1:{self.config['mqtt_port']}
""")
        self.log = (self.output / "ingress.log").open("w")
        try:
            self.process = subprocess.Popen([
                "docker", "run", "--rm", "--name", self.name, "--network", "host", "--security-opt", "label=disable",
                "--user", f"{os.getuid()}:{os.getgid()}",
                "--mount", f"type=bind,src={self.output},dst=/test,readonly",
                "haproxy:3.4.4-alpine", "haproxy", "-db", "-f", "/test/haproxy.cfg"],
                stdout=self.log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline and self.process.poll() is None:
                try:
                    with socket.create_connection((self.config["lan_address"], self.config["tls_port"]), timeout=.2):
                        return self
                except OSError:
                    time.sleep(.1)
            raise RuntimeError("cannot start record-bounded TLS ingress; see ingress.log and check Docker/LAN port")
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def __exit__(self, *_):
        if self.process:
            subprocess.run(["docker", "stop", "--time", "5", self.name],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
            self.process.wait(timeout=15)
        if self.log:
            self.log.close()
