// Exact-build, read-only report for Cafu's post-fire null render-object crash.
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

public class CafuFireCrashReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function function(long value) {
        Function exact = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        return exact != null ? exact : currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
    }

    private void decompile(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function target = function(value);
        println("");
        println("===== " + role + " target=" + address(value) +
            " function=" + (target == null ? "<none>" :
                target.getEntryPoint() + " " + target.getName(true)) +
            " =====");
        if (target == null || !seen.add(target.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            target, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void callers(long value, String role) {
        Function target = function(value);
        if (target == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            target.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() +
                " caller=" + (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
        }
    }

    private void references(long value, String role) {
        println("");
        println("REFERENCES role=" + role + " target=" + address(value));
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(address(value));
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() +
                " type=" + reference.getReferenceType() + " owner=" +
                (owner == null ? "<none>" :
                    owner.getEntryPoint() + " " + owner.getName(true)));
        }
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP Cafu fire crash report");
        println("SHA256=" + actual);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] targets = {
            0x00510d40L, 0x00510f02L,
            0x00536560L, 0x00536880L, 0x00536a40L,
            0x004cbe80L, 0x0054df80L,
            0x004c6de0L, 0x004c7160L, 0x004c87b0L,
            0x005861c0L, 0x00586e10L, 0x00587970L, 0x00587a80L,
            0x00418140L, 0x00418300L, 0x00418460L,
            0x00418760L, 0x00418a80L, 0x00418b90L,
            0x00523500L, 0x00540730L, 0x00540920L
        };
        String[] roles = {
            "CPosition attachment transform update", "fault instruction",
            "crash-stack component update", "crash-stack virtual dispatch",
            "crash-stack transform resolver",
            "crash-stack component loop", "character component update",
            "missile selection", "missile launch", "missile fire path",
            "missile construction", "missile initialization",
            "MissileData constructor", "MissileData serializer",
            "parent SFX setup", "matrix SFX setup", "position SFX setup",
            "SFX resource resolver", "SFX submit variant", "position SFX submit",
            "model-wrapper constructor", "HOM resource constructor",
            "HOM load completion"
        };
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler, targets[index], roles[index], seen);
            callers(targets[index], roles[index]);
        }

        // The live stack dispatches through the component at character+0x14c8.
        // Nearby RTTI/vtable references identify which authored subsystem owns it.
        references(0x006d0000L, "RTTI/vtable neighborhood marker");
        decompiler.dispose();
    }
}
