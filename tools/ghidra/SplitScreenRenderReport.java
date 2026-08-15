// Reports the exact-build Direct3D9 frame, viewport, and camera submission path.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

public class SplitScreenRenderReport extends GhidraScript {
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
            function.getEntryPoint() + " " + function.getName(true));
        Set<Address> callerEntries = new HashSet<Address>();
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
            if (caller != null && callerEntries.add(caller.getEntryPoint())) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    private Set<Address> reportReferences(
            long value,
            String role) {
        Address target = address(value);
        Set<Address> functions = new HashSet<Address>();
        println("");
        println("REFERENCES role=" + role + " target=" + target);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            if (caller != null) {
                functions.add(caller.getEntryPoint());
            }
        }
        return functions;
    }

    private Long matchingD3D9Offset(Instruction instruction) {
        long[] offsets = {0x44L, 0xa4L, 0xa8L, 0xacL, 0xbcL, 0x12cL};
        for (int operand = 0; operand < instruction.getNumOperands(); ++operand) {
            Object[] objects = instruction.getOpObjects(operand);
            for (Object object : objects) {
                if (!(object instanceof Scalar)) {
                    continue;
                }
                long value = ((Scalar)object).getUnsignedValue();
                for (long offset : offsets) {
                    if (value == offset) {
                        return offset;
                    }
                }
            }
            Scalar scalar = instruction.getScalar(operand);
            if (scalar == null) {
                continue;
            }
            long value = scalar.getUnsignedValue();
            for (long offset : offsets) {
                if (value == offset) {
                    return offset;
                }
            }
        }
        return null;
    }

    private String d3d9Method(long offset) {
        if (offset == 0x44L) return "IDirect3DDevice9::Present";
        if (offset == 0xa4L) return "IDirect3DDevice9::BeginScene";
        if (offset == 0xa8L) return "IDirect3DDevice9::EndScene";
        if (offset == 0xacL) return "IDirect3DDevice9::Clear";
        if (offset == 0xbcL) return "IDirect3DDevice9::SetViewport";
        if (offset == 0x12cL) return "IDirect3DDevice9::SetScissorRect";
        return "unknown";
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP split-screen render report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] knownFunctions = {
            0x004272e0L, // CCameraManager::GELGetRenderCamera
            0x00436fb0L, // CCameraManager::SetRenderCamera
            0x0041a630L, // ClearScreen implementation
            0x0040a5b0L, // world/scene render pass
            0x0040a820L, // world post-render helper
            0x005d4750L, // renderer phase helper
            0x005d4820L, // renderer phase helper
            0x005d48c0L  // renderer phase helper
        };
        String[] knownRoles = {
            "CCameraManager::GELGetRenderCamera",
            "CCameraManager::SetRenderCamera",
            "ClearScreen implementation",
            "world/scene render pass",
            "world post-render helper",
            "renderer phase helper 0x5d4750",
            "renderer phase helper 0x5d4820",
            "renderer phase helper 0x5d48c0"
        };
        for (int index = 0; index < knownFunctions.length; ++index) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(
                address(knownFunctions[index]));
            decompile(decompiler, function, knownRoles[index], seen);
            reportCallers(decompiler, function, knownRoles[index], seen);
        }

        String[] wantedNames = {
            "getrenderable", "getscenemanager", "clearscreen",
            "setscreenoffset", "setscreenscale", "allowgamerender",
            "enablegamerender", "getnearclip", "getfarclip"
        };
        println("");
        println("NAMED RENDER FUNCTIONS");
        FunctionIterator functions = currentProgram.getFunctionManager()
            .getFunctions(true);
        while (functions.hasNext() && !monitor.isCancelled()) {
            Function function = functions.next();
            String lower = function.getName(true).toLowerCase();
            boolean wanted = false;
            for (String name : wantedNames) {
                if (lower.contains(name)) {
                    wanted = true;
                    break;
                }
            }
            if (!wanted) {
                continue;
            }
            println("  function=" + function.getEntryPoint() + " name=" +
                function.getName(true));
            decompile(decompiler, function, "named render function", seen);
            reportCallers(decompiler, function, "named render function", seen);
        }

        Set<Address> sceneManagerUsers = reportReferences(
            0x00808d58L, "scene-manager global");
        reportReferences(0x00809d7cL, "camera-manager global");
        Set<Address> d3dDeviceUsers = reportReferences(
            0x007c31dcL, "IDirect3DDevice9 global");

        println("");
        println("SCENE-MANAGER USERS");
        for (Address entry : sceneManagerUsers) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
            decompile(decompiler, function, "scene-manager user", seen);
        }

        MemoryBlock textBlock = currentProgram.getMemory().getBlock(".text");
        if (textBlock == null) {
            throw new Exception("Executable has no .text block");
        }

        Map<Address, Set<String>> functionsByCall =
            new LinkedHashMap<Address, Set<String>>();
        Map<Address, Function> matchedFunctions =
            new LinkedHashMap<Address, Function>();
        InstructionIterator instructions = currentProgram.getListing()
            .getInstructions(new AddressSet(textBlock.getStart(), textBlock.getEnd()), true);
        println("");
        println("D3D9 INDIRECT CALLS");
        while (instructions.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = instructions.next();
            if (!instruction.getFlowType().isCall()) {
                continue;
            }
            Long offset = matchingD3D9Offset(instruction);
            if (offset == null) {
                continue;
            }
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(instruction.getAddress());
            String method = d3d9Method(offset);
            println("  callsite=" + instruction.getAddress() + " method=" + method +
                " instruction=" + instruction + " function=" +
                (function == null ? "<none>" : function.getEntryPoint() + " " +
                    function.getName(true)));
            if (function == null) {
                continue;
            }
            matchedFunctions.put(function.getEntryPoint(), function);
            functionsByCall.computeIfAbsent(function.getEntryPoint(),
                ignored -> new HashSet<String>()).add(method);
        }

        println("");
        println("D3D9 DEVICE-GLOBAL METHOD CANDIDATES");
        for (Address entry : d3dDeviceUsers) {
            Function function = currentProgram.getFunctionManager().getFunctionAt(entry);
            if (function == null) {
                continue;
            }
            boolean matched = false;
            InstructionIterator functionInstructions = currentProgram.getListing()
                .getInstructions(function.getBody(), true);
            while (functionInstructions.hasNext() && !monitor.isCancelled()) {
                Instruction instruction = functionInstructions.next();
                Long offset = matchingD3D9Offset(instruction);
                if (offset == null) {
                    continue;
                }
                matched = true;
                println("  site=" + instruction.getAddress() + " method-slot=" +
                    d3d9Method(offset) + " instruction=" + instruction +
                    " function=" + function.getEntryPoint() + " " +
                    function.getName(true));
            }
            if (matched) {
                decompile(decompiler, function,
                    "D3D9 device-global method candidate", seen);
                reportCallers(decompiler, function,
                    "D3D9 device-global method candidate", seen);
            }
        }

        println("");
        println("D3D9 CALL FUNCTIONS");
        for (Map.Entry<Address, Function> entry : matchedFunctions.entrySet()) {
            Function function = entry.getValue();
            println("  function=" + function.getEntryPoint() + " name=" +
                function.getName(true) + " methods=" +
                functionsByCall.get(entry.getKey()));
            decompile(decompiler, function, "D3D9 call function " +
                functionsByCall.get(entry.getKey()), seen);
            reportCallers(decompiler, function, "D3D9 call function " +
                functionsByCall.get(entry.getKey()), seen);
        }

        decompiler.dispose();
    }
}
