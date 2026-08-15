// Reports Sudeki's exact-build named-camera lifecycle for a Player 2 camera.
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

public class DualCameraReport extends GhidraScript {
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
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void reportCallers(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            function.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            decompile(decompiler, caller, "caller of " + role, seen);
        }
    }

    private void reportCallees(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null) {
            return;
        }
        println("");
        println("CALLEES role=" + role + " function=" +
            function.getEntryPoint());
        Set<Address> destinations = new HashSet<Address>();
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(function.getBody(), true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            for (Reference reference : instruction.getReferencesFrom()) {
                if (!reference.getReferenceType().isCall() ||
                    !destinations.add(reference.getToAddress())) {
                    continue;
                }
                Function callee = currentProgram.getFunctionManager()
                    .getFunctionAt(reference.getToAddress());
                println("  callsite=" + instruction.getAddress() +
                    " destination=" + reference.getToAddress() + " name=" +
                    (callee == null ? "<none>" : callee.getName(true)));
                decompile(decompiler, callee, "callee of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP dual-camera report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        long[] targets = {
            0x00436c10L, // CCameraManager::AddCamera
            0x00436de0L, // CCameraManager::RemoveCamera
            0x00436ed0L, // CCameraManager::GELGetCamera
            0x004272e0L, // CCameraManager::GELGetRenderCamera
            0x00436f30L, // CCameraManager::IsRenderCamera
            0x00436fb0L, // CCameraManager::SetRenderCamera
            0x00437100L, // CCameraManager::CameraExists
            0x00437170L, // CCameraManager::SetCameraTarget
            0x004369c0L, // default-camera initialization
            0x00436a30L  // camera-manager teardown
        };
        String[] roles = {
            "CCameraManager::AddCamera",
            "CCameraManager::RemoveCamera",
            "CCameraManager::GELGetCamera",
            "CCameraManager::GELGetRenderCamera",
            "CCameraManager::IsRenderCamera",
            "CCameraManager::SetRenderCamera",
            "CCameraManager::CameraExists",
            "CCameraManager::SetCameraTarget",
            "default-camera initialization",
            "camera-manager teardown"
        };

        for (int index = 0; index < targets.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(targets[index]));
            decompile(decompiler, function, roles[index], seen);
            reportCallers(decompiler, function, roles[index], seen);
            if (index == 0 || index == 1 || index == 7 || index == 8) {
                reportCallees(decompiler, function, roles[index], seen);
            }
        }
        decompiler.dispose();
    }
}
