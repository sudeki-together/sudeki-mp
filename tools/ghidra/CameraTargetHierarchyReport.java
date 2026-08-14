// Reports Camera::Target RTTI/vtables and the game-camera-mode users.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class CameraTargetHierarchyReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void reportReferences(
            DecompInterface decompiler,
            Address target,
            String role,
            boolean decompileCallers,
            Set<Address> seen) {
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("");
        println("REFERENCES role=" + role + " target=" + target);
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            if (decompileCallers && caller != null) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    private void dumpVtable(Symbol symbol) throws Exception {
        Memory memory = currentProgram.getMemory();
        Address table = symbol.getAddress();
        println("");
        println("VTABLE name=" + symbol.getName(true) + " address=" + table);
        for (int slot = 0; slot < 20; ++slot) {
            Address entryAddress = table.add(slot * 4L);
            long raw = Integer.toUnsignedLong(memory.getInt(entryAddress));
            Address destination = address(raw);
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(destination);
            if (function == null) {
                println("  slot=0x" + Integer.toHexString(slot * 4) +
                    " destination=" + destination + " name=<none>");
                if (slot > 3) {
                    break;
                }
            }
            else {
                println("  slot=0x" + Integer.toHexString(slot * 4) +
                    " destination=" + destination + " name=" +
                    function.getName(true));
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP camera-target hierarchy report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] functions = {
            0x004237b0L, // shared character-control reassignment
            0x004e84c0L, // install one Camera::Target slot
            0x00534b30L, // target constructor/factory
            0x00534b90L, // target constructor/factory
            0x00535040L, // target cache lookup/create
            0x00535100L  // derived target construction
        };
        String[] roles = {
            "shared character-control reassignment",
            "camera target slot installer",
            "first camera target constructor",
            "second camera target constructor",
            "camera target cache lookup/create",
            "derived camera target construction"
        };
        for (int index = 0; index < functions.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(functions[index]));
            decompile(decompiler, function, roles[index], seen);
            reportReferences(decompiler, address(functions[index]), roles[index],
                false, seen);
        }

        reportReferences(decompiler, address(0x00808da8L),
            "CGameCameraMode singleton", true, seen);

        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String fullName = symbol.getName(true);
            String lower = fullName.toLowerCase();
            if (!lower.contains("camera::") ||
                !lower.contains("target::vftable")) {
                continue;
            }
            dumpVtable(symbol);
            reportReferences(decompiler, symbol.getAddress(), fullName, true, seen);
        }

        decompiler.dispose();
    }
}
