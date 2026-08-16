// Reports Sudeki's exact-build ANIMID debug-name table.
// Read-only: refuses any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class AnimationIdNamesReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private static final long NAME_TABLE = 0x007363e8L;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private String readString(Address start) throws Exception {
        StringBuilder result = new StringBuilder();
        for (int index = 0; index < 256; ++index) {
            int value = currentProgram.getMemory().getByte(start.add(index)) & 0xff;
            if (value == 0) {
                break;
            }
            if (value < 0x20 || value > 0x7e) {
                return "<non-ascii>";
            }
            result.append((char)value);
        }
        return result.toString();
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP animation-ID names report");
        println("SHA256=" + actualSha256);
        for (int index = 0; index <= 0xc0; ++index) {
            long pointer = Integer.toUnsignedLong(
                currentProgram.getMemory().getInt(
                    address(NAME_TABLE + index * 4L)
                )
            );
            if (pointer == 0) {
                println(String.format("0x%02X <null>", index));
                continue;
            }
            println(String.format(
                "0x%02X %s",
                index,
                readString(address(pointer))
            ));
        }
    }
}
