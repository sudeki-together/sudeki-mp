// Reports the exact resource-table entries used by Sudeki's five title buttons.
// Read-only and hash-gated to the supported GOG executable.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.scalar.Scalar;

public class TitleMenuAssetReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private static final long RESOURCE_RECORDS = 0x007ca570L;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private long u32(long value) throws Exception {
        return Integer.toUnsignedLong(getInt(address(value)));
    }

    private String ascii(long value) throws Exception {
        StringBuilder result = new StringBuilder();
        for (int index = 0; index < 192; ++index) {
            int character = Byte.toUnsignedInt(getByte(address(value + index)));
            if (character == 0) {
                break;
            }
            if (character < 0x20 || character > 0x7e) {
                return "<non-ascii>";
            }
            result.append((char)character);
        }
        return result.toString();
    }

    private void reportTable(String label, long table) throws Exception {
        println("\nTABLE label=" + label + " address=" + address(table));
        for (int language = 0; language < 7; ++language) {
            long entryAddress = table + language * 4L;
            long index = u32(entryAddress);
            long recordAddress = RESOURCE_RECORDS + index * 12L;
            long packed = u32(recordAddress);
            long handle = u32(recordAddress + 4L);
            long refcount = u32(recordAddress + 8L);
            println(String.format(
                "  language=%d table_entry=%s index=%d record=%s " +
                "packed=0x%08X handle=0x%08X refcount_ptr=0x%08X",
                language, address(entryAddress), index, address(recordAddress),
                packed, handle, refcount));
        }
    }

    private void reportInitializerWindow() throws Exception {
        Function initializer = currentProgram.getFunctionManager()
            .getFunctionAt(address(0x005132b0L));
        if (initializer == null) {
            println("\nINITIALIZER_CALLS missing=true");
            return;
        }
        println("\nINITIALIZER_CALLS function=" + initializer.getEntryPoint());
        InstructionIterator iterator = currentProgram.getListing()
            .getInstructions(initializer.getBody(), true);
        int resourceOrdinal = 0;
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = iterator.next();
            if (!"CALL".equals(instruction.getMnemonicString()) ||
                instruction.getNumOperands() == 0) {
                continue;
            }
            Object[] objects = instruction.getOpObjects(0);
            if (objects.length != 1 || !(objects[0] instanceof Address) ||
                !objects[0].equals(address(0x005b9440L))) {
                continue;
            }
            Instruction source = instruction.getPrevious();
            source = source == null ? null : source.getPrevious();
            Instruction type = source == null ? null : source.getPrevious();
            Instruction destination = instruction.getNext();
            Scalar destinationScalar = destination == null ? null :
                destination.getScalar(1);
            long destinationValue = destinationScalar == null ? 0L :
                destinationScalar.getUnsignedValue();
            if (destinationValue >= 0x007cb674L &&
                destinationValue <= 0x007cb80cL) {
                Scalar sourceScalar = source == null ? null :
                    source.getScalar(1);
                long sourceValue = sourceScalar == null ? 0L :
                    sourceScalar.getUnsignedValue();
                println("  resource_ordinal=" + resourceOrdinal +
                    " prepare_call=" + instruction.getAddress() +
                    " type_setup={" + type + "}" +
                    " source=" + address(sourceValue) +
                    " source_text=\"" + ascii(sourceValue) + "\"" +
                    " destination=" + address(destinationValue));
            }
            resourceOrdinal++;
        }
        println("  resource_prepare_call_count=" + resourceOrdinal);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP title-menu asset report");
        println("SHA256=" + actual);
        println("ImageBase=" + currentProgram.getImageBase());
        println("ResourceRecords=" + address(RESOURCE_RECORDS));

        reportTable("Continue", 0x006c2becL);
        reportTable("NewGame", 0x006c2c08L);
        reportTable("Options", 0x006c2c24L);
        reportTable("Credits", 0x006c2c40L);
        reportTable("QuitGame", 0x006c2c5cL);
        reportInitializerWindow();

    }
}
