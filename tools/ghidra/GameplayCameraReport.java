// Reports the native free-gameplay camera configuration path.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public class GameplayCameraReport extends GhidraScript {
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

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP gameplay-camera report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[][] targets = {
            {0x004277b0L, 0}, // character input-event handler
            {0x00427cf0L, 0}, // character controller frame update
            {0x00404fa0L, 0}, // CGroupPlayers::InCombat
            {0x00429400L, 0}, // GetGamePadControlComponent
            {0x00438c40L, 0}, // GetCameraManager
            {0x004375f0L, 0}, // CCameraManager::LoadConfig
            {0x00437cd0L, 0}, // CCameraManager::SetCameraConfig
            {0x004e8d50L, 0}, // CCamera::GetConfigFloat
            {0x005188f0L, 0}, // config-name lookup
            {0x00518580L, 0}, // config float lookup
            {0x00518850L, 0}, // config field-offset lookup
            {0x00808da4L, 0}, // character controller global
            {0x00809d7cL, 0}, // camera manager global
            {0x006d1b40L, 0}, // ExplorationDefaultDistance
            {0x006d1b5cL, 0}, // ExplorationAbsMaxFlatDistance
            {0x006d1b9cL, 0}, // ExplorationMaxFlatDistance
            {0x006d1bb8L, 0}, // ExplorationMinFlatDistance
            {0x006d1914L, 0}, // ExplorationUserDistanceScale
            {0x006d1458L, 0}, // CombatDefaultDistance
            {0x006d1470L, 0}, // CombatAbsMaxFlatDistance
            {0x006d14a8L, 0}, // CombatMaxFlatDistance
            {0x006d14c0L, 0}, // CombatMinFlatDistance
            {0x006d12a0L, 0}, // CombatUserDistanceScale
            {0x006d1194L, 0}, // CombatVisibilityRotScale
            {0x006d1180L, 0}, // CombatZoomInDelay
            {0x006d116cL, 0}, // CombatZoomOutDelay
            {0x006d1158L, 0}, // CombatZoomInSpeed
            {0x006d1144L, 0}  // CombatZoomOutSpeed
        };
        String[] roles = {
            "character input-event handler",
            "character controller frame update",
            "CGroupPlayers::InCombat",
            "GetGamePadControlComponent",
            "GetCameraManager",
            "CCameraManager::LoadConfig",
            "CCameraManager::SetCameraConfig",
            "CCamera::GetConfigFloat",
            "camera config-name lookup",
            "camera config float lookup",
            "camera config field-offset lookup",
            "character controller global",
            "camera manager global",
            "ExplorationDefaultDistance",
            "ExplorationAbsMaxFlatDistance",
            "ExplorationMaxFlatDistance",
            "ExplorationMinFlatDistance",
            "ExplorationUserDistanceScale",
            "CombatDefaultDistance",
            "CombatAbsMaxFlatDistance",
            "CombatMaxFlatDistance",
            "CombatMinFlatDistance",
            "CombatUserDistanceScale",
            "CombatVisibilityRotScale",
            "CombatZoomInDelay",
            "CombatZoomOutDelay",
            "CombatZoomInSpeed",
            "CombatZoomOutSpeed"
        };

        for (int index = 0; index < targets.length; ++index) {
            reportReferences(decompiler, targets[index][0], roles[index],
                true, seen);
            if (index < 11) {
                decompile(decompiler,
                    currentProgram.getFunctionManager().getFunctionAt(
                        address(targets[index][0])), roles[index], seen);
            }
        }

        Map<Address, Integer> actionMasks = new HashMap<Address, Integer>();
        Map<Address, Integer> controllerFieldMasks =
            new HashMap<Address, Integer>();
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(
                new AddressSet(
                    currentProgram.getMemory().getBlock(".text").getStart(),
                    currentProgram.getMemory().getBlock(".text").getEnd()),
                true);
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(instruction.getAddress());
            if (function == null) {
                continue;
            }
            int mask = actionMasks.getOrDefault(function.getEntryPoint(), 0);
            int fieldMask = controllerFieldMasks.getOrDefault(
                function.getEntryPoint(), 0);
            for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
                Scalar scalar = instruction.getScalar(operand);
                if (scalar == null) {
                    continue;
                }
                long value = scalar.getUnsignedValue();
                if (value == 0x69L) mask |= 1;
                if (value == 0x6aL) mask |= 2;
                if (value == 0x2fL) mask |= 4;
                if (value == 0x30L) mask |= 8;
                if (value == 0x184L) fieldMask |= 1;
                if (value == 0x188L) fieldMask |= 2;
            }
            actionMasks.put(function.getEntryPoint(), mask);
            controllerFieldMasks.put(function.getEntryPoint(), fieldMask);
        }
        println("");
        println("FUNCTIONS REFERENCING ACTION PAIRS");
        for (Map.Entry<Address, Integer> entry : actionMasks.entrySet()) {
            int mask = entry.getValue();
            if ((mask & 3) != 3 && (mask & 12) != 12) {
                continue;
            }
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(entry.getKey());
            if (function == null ||
                entry.getKey().getUnsignedOffset() >= 0x00690000L ||
                function.getBody().getNumAddresses() > 0x2000L) {
                continue;
            }
            println("  function=" + entry.getKey() + " mask=0x" +
                Integer.toHexString(mask) + " name=" +
                (function == null ? "<none>" : function.getName(true)));
            decompile(decompiler, function, "camera/weapon action-pair consumer", seen);
        }

        println("");
        println("FUNCTIONS REFERENCING CAMERA CONTROLLER FIELDS");
        for (Map.Entry<Address, Integer> entry : controllerFieldMasks.entrySet()) {
            if (entry.getValue() == 0) {
                continue;
            }
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(entry.getKey());
            if (function == null ||
                entry.getKey().getUnsignedOffset() >= 0x00690000L ||
                function.getBody().getNumAddresses() > 0x3000L) {
                continue;
            }
            println("  function=" + entry.getKey() + " mask=0x" +
                Integer.toHexString(entry.getValue()) + " name=" +
                function.getName(true));
            decompile(decompiler, function,
                "camera controller-field consumer", seen);
        }

        decompiler.dispose();
    }
}
