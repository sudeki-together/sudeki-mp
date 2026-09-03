// Reports the exact native Ailish ranged-fire and missile-manager seams used
// by the LAN arena prototype. The script is read-only and refuses an
// unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class LanArenaAilishRangedReport extends GhidraScript {
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
        if (function == null || !seen.add(function.getEntryPoint())) return;
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

    private void decompileAt(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionContaining(address(value)),
            role, seen);
    }

    private void reportReferences(
            DecompInterface decompiler,
            long value,
            String role,
            boolean decompileCallers,
            Set<Address> seen) {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("");
        println("REFERENCES role=" + role + " target=" + target);
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
            if (decompileCallers && caller != null) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP LAN Ailish ranged report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        decompileAt(decompiler, 0x00534410L,
            "first-person held-fire gate", seen);
        decompileAt(decompiler, 0x004c9d80L,
            "first-person weapon fire", seen);
        decompileAt(decompiler, 0x004c9e60L,
            "first-person mode transition", seen);
        decompileAt(decompiler, 0x004ca6d0L,
            "post-fire helper", seen);
        decompileAt(decompiler, 0x004cabb0L,
            "ranged animation request", seen);
        decompileAt(decompiler, 0x004c7990L,
            "CMissileManager::IsFiring", seen);
        decompileAt(decompiler, 0x004c79a0L,
            "CMissileManager::CanFire", seen);
        decompileAt(decompiler, 0x004c7160L,
            "native selected-missile launch", seen);

        reportReferences(decompiler, 0x004c9d80L,
            "first-person weapon fire", true, seen);
        reportReferences(decompiler, 0x004c7160L,
            "native selected-missile launch", true, seen);
        reportReferences(decompiler, 0x006d4c8cL,
            "CMissileManager primary vtable", true, seen);

        println("");
        println("MATCHING RANGED SYMBOLS");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if (!(lower.contains("missile") || lower.contains("weapon"))) continue;
            if (!(lower.contains("fire") || lower.contains("launch") ||
                  lower.contains("queue") || lower.contains("canfire") ||
                  lower.contains("isfiring"))) continue;
            println("  " + symbol.getAddress() + " " + symbol.getName(true) +
                " type=" + symbol.getSymbolType());
        }
        decompiler.dispose();
    }
}
