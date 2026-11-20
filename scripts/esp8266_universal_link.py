Import("env")

# The fixed universal sketch has no libc printf/scanf float conversions.
# Keep Arduino's other forced symbols, including the boot entry and vectors.
link_flags = env["LINKFLAGS"]
for symbol in ("_printf_float", "_scanf_float"):
    index = link_flags.index(symbol)
    if index == 0 or link_flags[index - 1] != "-u":
        raise RuntimeError(f"Unexpected ESP8266 linker flags for {symbol}")
    del link_flags[index - 1:index + 1]
