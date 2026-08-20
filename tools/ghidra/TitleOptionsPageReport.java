// Reports the exact native UILayerOptionsMenu class used as a real title
// front-end subpage. Read-only and hash-gated to the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class TitleOptionsPageReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private static final long VTABLE = 0x006d1cb4L;
    private static final int VTABLE_BYTES = 0x6c;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private long pointer(Memory memory, long value) throws Exception {
        return Integer.toUnsignedLong(memory.getInt(address(value)));
    }

    private void reportFunction(DecompInterface decompiler, Function function,
            String role, Set<Address> seen) {
        println("\n===== " + role + " =====");
        if (function == null) {
            println("function_missing=true");
            return;
        }
        println("function=" + function.getEntryPoint() + " " +
            function.getName(true));
        if (seen.add(function.getEntryPoint())) {
            DecompileResults result = decompiler.decompileFunction(function,
                90, monitor);
            if (result.decompileCompleted()) {
                println(result.getDecompiledFunction().getC());
            } else {
                println("decompile_error=" + result.getErrorMessage());
            }
        } else {
            println("already_decompiled=true");
        }
    }

    private void reportCallers(DecompInterface decompiler, Address target,
            String role, Set<Address> seen) {
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        int index = 0;
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            if (!ref.getReferenceType().isCall()) {
                continue;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("caller[" + index + "]=" + ref.getFromAddress() +
                " owner=" + (owner == null ? "<none>" :
                    owner.getEntryPoint() + " " + owner.getName(true)));
            reportFunction(decompiler, owner, role + " caller " + index,
                seen);
            ++index;
        }
        println(role + "_caller_count=" + index);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }

        println("SudekiMP native Options-page report");
        println("SHA256=" + actual);
        println("class=UILayerOptionsMenu");
        println("vtable=" + address(VTABLE));

        Memory memory = currentProgram.getMemory();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        for (int offset = 0; offset < VTABLE_BYTES; offset += 4) {
            long target = pointer(memory, VTABLE + offset);
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(target));
            println("slot=0x" + Integer.toHexString(offset) +
                " target=" + address(target) + " function=" +
                (function == null ? "<missing>" : function.getName(true)));
            reportFunction(decompiler, function,
                "UILayerOptionsMenu vtable+0x" + Integer.toHexString(offset),
                seen);
        }

        Function constructor = currentProgram.getFunctionManager()
            .getFunctionAt(address(0x0051a7b0L));
        Function destructor = currentProgram.getFunctionManager()
            .getFunctionAt(address(0x0051c050L));
        reportFunction(decompiler, constructor,
            "UILayerOptionsMenu constructor", seen);
        reportCallers(decompiler, address(0x0051a7b0L),
            "constructor", seen);
        reportFunction(decompiler, destructor,
            "UILayerOptionsMenu destructor", seen);

        long[] supportingFunctions = {
            0x0051a960L, // Options data/presentation-table constructor
            0x0051d9a0L, // post-initialization Options-row presentation setup
            0x0051cdb0L, // Options-page activation/presentation setup
            0x0051e4a0L, // Options selection refresh
            0x00520370L, // shared front-end presentation state helper
            0x0055b150L  // shared UI-element state helper
        };
        String[] supportingRoles = {
            "Options data presentation-table constructor",
            "Options row presentation setup",
            "Options page activation setup",
            "Options selection refresh",
            "front-end presentation state helper",
            "UI-element state helper"
        };
        for (int index = 0; index < supportingFunctions.length; ++index) {
            Function supporting = currentProgram.getFunctionManager()
                .getFunctionAt(address(supportingFunctions[index]));
            reportFunction(decompiler, supporting, supportingRoles[index],
                seen);
            reportCallers(decompiler, address(supportingFunctions[index]),
                supportingRoles[index], seen);
        }

        ReferenceIterator vtableRefs = currentProgram.getReferenceManager()
            .getReferencesTo(address(VTABLE));
        int vtableRefCount = 0;
        while (vtableRefs.hasNext() && !monitor.isCancelled()) {
            Reference ref = vtableRefs.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("vtable_ref[" + vtableRefCount + "]=" +
                ref.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            ++vtableRefCount;
        }
        println("vtable_ref_count=" + vtableRefCount);
        decompiler.dispose();
    }
}
