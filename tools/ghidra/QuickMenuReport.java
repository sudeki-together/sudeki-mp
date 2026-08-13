// Reports the static anchors for SudekiMP's Quick Menu slowdown research.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class QuickMenuReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void reportInstruction(long value, String role) throws Exception {
        Address address = address(value);
        Instruction instruction = currentProgram.getListing().getInstructionAt(address);
        if (instruction == null) {
            throw new Exception("No instruction at " + address + " (" + role + ")");
        }

        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        String functionName = function == null ? "<none>" : function.getName();
        println(String.format("%-30s %s  %-42s function=%s",
            role, address, instruction.toString(), functionName));
    }

    private void reportFloat(long value, String role) throws Exception {
        Address address = address(value);
        Memory memory = currentProgram.getMemory();
        int bits = memory.getInt(address);
        println(String.format("%-30s %s  %.9g (bits=0x%08X)",
            role, address, Float.intBitsToFloat(bits), bits));
    }

    private void reportReferences(long value, String role) {
        Address address = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(address);
        int count = 0;
        StringBuilder from = new StringBuilder();
        while (references.hasNext()) {
            Reference reference = references.next();
            if (count < 12) {
                if (from.length() != 0) {
                    from.append(", ");
                }
                from.append(reference.getFromAddress());
            }
            count++;
        }
        println(String.format("%-30s %s  references=%d from=[%s]",
            role, address, count, from));
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Quick Menu static report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());
        println("");

        reportInstruction(0x00498ec0L, "Quick Menu open function");
        reportInstruction(0x00498ef0L, "Request mode 1");
        reportInstruction(0x00499180L, "Quick Menu close function");
        reportInstruction(0x004991b6L, "Request mode 0");
        reportInstruction(0x0068dd10L, "Compare current/requested");
        reportInstruction(0x0068dd30L, "Load fixed slow scale");
        reportInstruction(0x0068dd3dL, "Load normal scale");
        reportInstruction(0x0068dd86L, "Apply scale to delta");
        reportInstruction(0x0068dd92L, "Check full pause");
        reportInstruction(0x0068dcafL, "Apply master scale");
        println("");

        reportFloat(0x006c4018L, "Fixed slow scale");
        reportFloat(0x00745f70L, "Normal speed");
        reportFloat(0x00725810L, "Master speed");
        println("");

        reportReferences(0x006c4018L, "Fixed slow scale xrefs");
        reportReferences(0x00745f70L, "Normal speed xrefs");
        reportReferences(0x00808da0L, "CGameSpeed singleton xrefs");
    }
}
