// Reports Sudeki's camera update/render ownership split for a safe Player 2 view.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class CameraRenderIsolationReport extends GhidraScript {
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

    private void reportVtable(
            DecompInterface decompiler,
            Set<Address> seen) throws Exception {
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String qualified = symbol.getName(true);
            if (!qualified.toLowerCase().contains("ccamera") ||
                !qualified.toLowerCase().contains("vftable")) {
                continue;
            }
            Address table = symbol.getAddress();
            println("");
            println("VTABLE symbol=" + qualified + " address=" + table);
            for (int offset = 0; offset < 0x70; offset += 4) {
                long pointer = Integer.toUnsignedLong(getInt(table.add(offset)));
                Address destination = address(pointer);
                Function method = currentProgram.getFunctionManager()
                    .getFunctionAt(destination);
                println("  offset=0x" + Integer.toHexString(offset) +
                    " pointer=" + destination + " method=" +
                    (method == null ? "<none>" : method.getName(true)));
                if (method != null) {
                    decompile(decompiler, method,
                        "CCamera vtable+0x" + Integer.toHexString(offset), seen);
                }
            }
        }
    }

    private void reportFrameWindow(long start, long end) {
        println("");
        println("FRAME WINDOW start=" + address(start) + " end=" + address(end));
        Instruction instruction = currentProgram.getListing()
            .getInstructionAt(address(start));
        if (instruction == null) {
            instruction = currentProgram.getListing()
                .getInstructionAfter(address(start));
        }
        while (instruction != null &&
               instruction.getAddress().getUnsignedOffset() <= end &&
               !monitor.isCancelled()) {
            println("  " + instruction.getAddress() + " " + instruction);
            instruction = instruction.getNext();
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP camera render-isolation report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        long[] targets = {
            0x00436c10L, // CCameraManager::AddCamera
            0x00436fb0L, // CCameraManager::SetRenderCamera
            0x00437170L, // CCameraManager::SetCameraTarget
            0x004374b0L, // CCameraManager::GetCameraMode
            0x00437530L, // CCameraManager::SetCameraState
            0x00437cd0L, // CCameraManager::SetCameraConfig
            0x00438930L, // CCameraManager::CameraReturnToDefault
            0x00438990L, // CCameraManager::CameraSnapToDefault
            0x00438b30L, // camera-manager matrix/state accessor
            0x00438c50L, // camera-manager per-frame/input updater
            0x004e7110L, // CCamera constructor
            0x004e7320L, // CCamera reset/destructor-adjacent initialization
            0x004e7660L, // CCamera update-node frame update
            0x004e79a0L, // CCamera target/default-state setup
            0x004e7aa0L, // camera matrix/state application
            0x004e7f00L, // camera-manager-facing camera helper
            0x004e8320L, // current-camera state handoff
            0x004e8360L, // CCamera config application
            0x004e93b0L, // CCamera return/snap-to-default implementation
            0x004e9480L, // CCamera transformed-position application
            0x004e95b0L, // CCamera target/state application
            0x004e8e50L, // camera debug state reader
            0x00408690L, // active CCamera solve/update helper
            0x005d1370L, // camera render-state constructor
            0x0040a5b0L, // world render pass
            0x0068d3f0L  // main frame/render loop
        };
        String[] roles = {
            "CCameraManager::AddCamera",
            "CCameraManager::SetRenderCamera",
            "CCameraManager::SetCameraTarget",
            "CCameraManager::GetCameraMode",
            "CCameraManager::SetCameraState",
            "CCameraManager::SetCameraConfig",
            "CCameraManager::CameraReturnToDefault",
            "CCameraManager::CameraSnapToDefault",
            "camera-manager matrix/state accessor",
            "camera-manager per-frame/input updater",
            "CCamera constructor",
            "CCamera reset/destructor-adjacent initialization",
            "CCamera update-node frame update",
            "CCamera target/default-state setup",
            "camera matrix/state application",
            "camera-manager-facing camera helper",
            "current-camera state handoff",
            "CCamera config application",
            "CCamera return/snap-to-default implementation",
            "CCamera transformed-position application",
            "CCamera target/state application",
            "camera debug state reader",
            "active CCamera solve/update helper",
            "camera render-state constructor",
            "world render pass",
            "main frame/render loop"
        };
        for (int index = 0; index < targets.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(targets[index]));
            decompile(decompiler, function, roles[index], seen);
            reportCallers(decompiler, function, roles[index], seen);
        }

        reportVtable(decompiler, seen);
        reportFrameWindow(0x0068d420L, 0x0068d5b5L);
        decompiler.dispose();
    }
}
