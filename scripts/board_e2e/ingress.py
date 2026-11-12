"""Temporary, verified TLS ingress to an existing loopback development Engine."""

import asyncio
import ssl
import threading


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
    def __init__(self, output, config):
        self.output, self.config = output, config
        self.started = threading.Event()
        self.error = None
        self.thread = threading.Thread(target=self._thread, daemon=True)

    async def _connection(self, reader, writer):
        upstream = None
        try:
            remote, upstream = await asyncio.wait_for(
                asyncio.open_connection("127.0.0.1", self.config["engine_port"]), 5)

            async def pump(source, target):
                try:
                    while data := await source.read(8192):
                        target.write(data)
                        await target.drain()
                finally:
                    target.close()

            await asyncio.gather(pump(reader, upstream), pump(remote, writer))
        except (OSError, asyncio.TimeoutError):
            pass
        finally:
            writer.close()
            if upstream:
                upstream.close()

    async def _serve(self):
        self.loop = asyncio.get_running_loop()
        self.stop = asyncio.Event()
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(self.output / "server.crt", self.output / "server.key")
        server = await asyncio.start_server(
            self._connection, self.config["lan_address"], self.config["tls_port"],
            ssl=context, ssl_handshake_timeout=5)
        self.started.set()
        async with server:
            await self.stop.wait()

    def _thread(self):
        try:
            asyncio.run(self._serve())
        except Exception as error:
            self.error = error
            self.started.set()

    def __enter__(self):
        self.thread.start()
        if not self.started.wait(10) or self.error:
            raise RuntimeError("cannot start local TLS ingress; check LAN address and port")
        return self

    def __exit__(self, *_):
        self.loop.call_soon_threadsafe(self.stop.set)
        self.thread.join(10)
