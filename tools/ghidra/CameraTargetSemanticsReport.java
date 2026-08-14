// Reports the exact-build camera-target methods needed for a safe group target.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class CameraTargetSemanticsReport extends GhidraScript {
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

    private void reportReferences(Address target, String role) {
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
        }
    }

    private void reportDirectCallees(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null) {
            return;
        }
        println("");
        println("DIRECT CALLEES role=" + role + " function=" +
            function.getEntryPoint());
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        Set<Address> callees = new HashSet<Address>();
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall()) {
                    continue;
                }
                Address destination = reference.getToAddress();
                Function callee = currentProgram.getFunctionManager()
                    .getFunctionAt(destination);
                if (callee == null || !callees.add(destination)) {
                    continue;
                }
                println("  callsite=" + instruction.getAddress() +
                    " destination=" + destination + " name=" +
                    callee.getName(true));
                decompile(decompiler, callee, "direct callee of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP camera-target semantics report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] functions = {
            0x0042a370L, // CGameCameraMode character reassignment
            0x00534fb0L, // MatrixTarget cache/create
            0x004e7bb0L, // MatrixTarget creator caller
            0x00553950L, // MatrixTarget creator caller
            0x0059aa20L, // MatrixTarget destructor
            0x0059a6b0L, // MatrixTarget virtual +0x04
            0x0059a6d0L, // MatrixTarget virtual +0x0c
            0x0059a6e0L, // MatrixTarget virtual +0x10
            0x0059a720L, // MatrixTarget virtual +0x14
            0x0059a760L, // MatrixTarget virtual +0x18
            0x0059a7a0L, // MatrixTarget virtual +0x1c
            0x0059a7e0L, // MatrixTarget virtual +0x20
            0x0059a870L, // MatrixTarget virtual +0x2c
            0x0059a0d0L, // OffsetTarget virtual +0x04
            0x0059a120L, // OffsetTarget virtual +0x08
            0x0059a150L, // OffsetTarget virtual +0x0c
            0x0059a170L, // OffsetTarget virtual +0x10
            0x0059a180L, // OffsetTarget virtual +0x14
            0x0059a190L, // OffsetTarget virtual +0x1c/+0x20
            0x0059a1a0L, // OffsetTarget virtual +0x24
            0x0059a1c0L, // OffsetTarget virtual +0x28
            0x0059a1e0L, // OffsetTarget virtual +0x2c
            0x0059a2e0L, // GameObjectTarget virtual +0x0c
            0x0059a300L, // GameObjectTarget virtual +0x10
            0x0059a350L, // GameObjectTarget virtual +0x14
            0x0059a3e0L, // GameObjectTarget virtual +0x18
            0x0059a470L, // GameObjectTarget virtual +0x1c
            0x0059a500L, // GameObjectTarget virtual +0x20
            0x0059a5f0L  // GameObjectTarget virtual +0x2c
        };
        String[] roles = {
            "CGameCameraMode character reassignment",
            "MatrixTarget cache/create",
            "first MatrixTarget creator caller",
            "second MatrixTarget creator caller",
            "MatrixTarget destructor",
            "MatrixTarget virtual +0x04",
            "MatrixTarget virtual +0x0c",
            "MatrixTarget virtual +0x10",
            "MatrixTarget virtual +0x14",
            "MatrixTarget virtual +0x18",
            "MatrixTarget virtual +0x1c",
            "MatrixTarget virtual +0x20",
            "MatrixTarget virtual +0x2c",
            "OffsetTarget virtual +0x04",
            "OffsetTarget virtual +0x08",
            "OffsetTarget virtual +0x0c",
            "OffsetTarget virtual +0x10",
            "OffsetTarget virtual +0x14",
            "OffsetTarget virtual +0x1c/+0x20",
            "OffsetTarget virtual +0x24",
            "OffsetTarget virtual +0x28",
            "OffsetTarget virtual +0x2c",
            "GameObjectTarget virtual +0x0c",
            "GameObjectTarget virtual +0x10",
            "GameObjectTarget virtual +0x14",
            "GameObjectTarget virtual +0x18",
            "GameObjectTarget virtual +0x1c",
            "GameObjectTarget virtual +0x20",
            "GameObjectTarget virtual +0x2c"
        };

        for (int index = 0; index < functions.length; ++index) {
            Address entry = address(functions[index]);
            Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
            decompile(decompiler, function, roles[index], seen);
            reportReferences(entry, roles[index]);
            if (index < 4) {
                reportDirectCallees(decompiler, function, roles[index], seen);
            }
        }

        decompiler.dispose();
    }
}
