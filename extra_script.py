Import("env")


def without_forced_float_stdio(flags):
    """Let the linker keep float stdio only when code actually references it."""
    filtered = []
    index = 0
    flags = list(flags)
    while index < len(flags):
        if (
            flags[index] == "-u"
            and index + 1 < len(flags)
            and flags[index + 1] in ("_printf_float", "_scanf_float")
        ):
            index += 2
            continue
        filtered.append(flags[index])
        index += 1
    return filtered


# ESP8266 Arduino 2.7.4 forces both newlib float printf and scanf into every
# firmware. SmallDesktopDisplay uses TFT drawFloat/strtof, not stdio %f. Real
# references would still pull these symbols back through the normal linker.
env.Replace(LINKFLAGS=without_forced_float_stdio(env.get("LINKFLAGS", [])))

env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.elf",
    env.VerboseAction(" ".join([
        "$OBJCOPY", "-O", "ihex", "-R", ".eeprom",
        "$BUILD_DIR/${PROGNAME}.elf", "$BUILD_DIR/${PROGNAME}.hex"
    ]), "Building $BUILD_DIR/${PROGNAME}.hex")
)

