// Reports the exact-build gameplay-camera target handoff and its direct users.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class CameraTargetReport extends GhidraScript {
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

        println("SudekiMP camera-target report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[][] targets = {
            {0x00437170L, 1}, // CCameraManager::SetCameraTarget
            {0x00437b90L, 1}, // CCameraManager::IsCameraTargetValid
            {0x0042a8b0L, 1}, // GetGameCameraMode
            {0x004237b0L, 1}, // shared character-control reassignment
            {0x00438c40L, 0}, // GetCameraManager
            {0x004272e0L, 0}, // CCameraManager::GELGetRenderCamera
            {0x00436fb0L, 0}, // CCameraManager::SetRenderCamera
            {0x00438930L, 0}, // CCameraManager::CameraReturnToDefault
            {0x00438990L, 0}, // CCameraManager::CameraSnapToDefault
            {0x004389f0L, 0}, // CCameraManager::SetCameraPosition
            {0x00809d7cL, 0}, // camera manager global
            {0x00808da4L, 0}  // character controller global
        };
        String[] roles = {
            "CCameraManager::SetCameraTarget",
            "CCameraManager::IsCameraTargetValid",
            "GetGameCameraMode",
            "shared character-control reassignment",
            "GetCameraManager",
            "CCameraManager::GELGetRenderCamera",
            "CCameraManager::SetRenderCamera",
            "CCameraManager::CameraReturnToDefault",
            "CCameraManager::CameraSnapToDefault",
            "CCameraManager::SetCameraPosition",
            "camera manager global",
            "character controller global"
        };

        for (int index = 0; index < targets.length; ++index) {
            boolean callers = targets[index][1] != 0;
            reportReferences(decompiler, targets[index][0], roles[index], callers, seen);
            Function function = currentProgram.getFunctionManager().getFunctionAt(
                address(targets[index][0]));
            decompile(decompiler, function, roles[index], seen);
            if (index < 4) {
                reportDirectCallees(decompiler, function, roles[index], seen);
            }
        }

        println("");
        println("NAMED CAMERA/TARGET FUNCTIONS");
        FunctionIterator functions = currentProgram.getFunctionManager()
            .getFunctions(true);
        while (functions.hasNext() && !monitor.isCancelled()) {
            Function function = functions.next();
            String name = function.getName(true);
            String lower = name.toLowerCase();
            if ((lower.contains("camera") || lower.contains("lookat")) &&
                (lower.contains("target") || lower.contains("lookat") ||
                 lower.contains("default") || lower.contains("render"))) {
                println("  function=" + function.getEntryPoint() + " name=" + name);
            }
        }

        decompiler.dispose();
    }
}
