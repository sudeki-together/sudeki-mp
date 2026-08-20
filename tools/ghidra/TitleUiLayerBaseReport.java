// Finds the native UILayerSubMenu and UILayer RTTI/vtable boundaries used by
// the title Options page. Read-only and hash-gated to the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class TitleUiLayerBaseReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private long pointer(long value) throws Exception {
        return Integer.toUnsignedLong(currentProgram.getMemory()
            .getInt(address(value)));
    }

    private void decompile(DecompInterface decompiler, Function function,
            String role, Set<Address> seen) {
        println("\n===== " + role + " =====");
        if (function == null) {
            println("function_missing=true");
            return;
        }
        println("function=" + function.getEntryPoint() + " " +
            function.getName(true));
        if (!seen.add(function.getEntryPoint())) {
            println("already_decompiled=true");
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90,
            monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "decompile_error=" + result.getErrorMessage());
    }

    private void reportType(DecompInterface decompiler, long typeDescriptor,
            String typeName, Set<Address> seen) throws Exception {
        ReferenceIterator typeRefs = currentProgram.getReferenceManager()
            .getReferencesTo(address(typeDescriptor));
        int colIndex = 0;
        while (typeRefs.hasNext() && !monitor.isCancelled()) {
            Reference typeRef = typeRefs.next();
            long from = typeRef.getFromAddress().getOffset();
            if (from < 12 || pointer(from) != typeDescriptor) {
                continue;
            }
            long col = from - 12;
            if (pointer(col) != 0 || pointer(col + 4) != 0 ||
                pointer(col + 8) != 0) {
                continue;
            }
            println("\nclass=" + typeName + " col=" + address(col));
            ReferenceIterator colRefs = currentProgram.getReferenceManager()
                .getReferencesTo(address(col));
            int vtableIndex = 0;
            while (colRefs.hasNext() && !monitor.isCancelled()) {
                Reference colRef = colRefs.next();
                long slot = colRef.getFromAddress().getOffset();
                long vtable = slot + 4;
                println("vtable[" + vtableIndex + "]=" + address(vtable));
                for (int offset = 0; offset < 0x6c; offset += 4) {
                    long target = pointer(vtable + offset);
                    Function method = currentProgram.getFunctionManager()
                        .getFunctionAt(address(target));
                    if (method == null) {
                        break;
                    }
                    println("slot=0x" + Integer.toHexString(offset) +
                        " target=" + address(target) + " function=" +
                        method.getName(true));
                    decompile(decompiler, method, typeName + " vtable+0x" +
                        Integer.toHexString(offset), seen);
                }
                ++vtableIndex;
            }
            println("vtable_count=" + vtableIndex);
            ++colIndex;
        }
        println(typeName + "_col_count=" + colIndex);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP title UI base-layer report");
        println("SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        reportType(decompiler, 0x0075931cL, "UILayerSubMenu", seen);
        reportType(decompiler, 0x00759304L, "UILayer", seen);
        decompiler.dispose();
    }
}
