// Reports the exact-build Spirit Strike presentation and render-target/history paths.
// Read-only: refuses to inspect any executable other than the supported GOG build.
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

public class PureLandRenderHistoryReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function functionAtOrContaining(long value) {
        Address target = address(value);
        Function function = currentProgram.getFunctionManager().getFunctionAt(target);
        if (function == null) {
            function = currentProgram.getFunctionManager().getFunctionContaining(target);
        }
        return function;
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void callers(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen,
            boolean decompileCallers) {
        if (function == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " target=" + function.getEntryPoint());
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
            if (decompileCallers) {
                decompile(decompiler, caller, "caller of " + role, seen);
            }
        }
    }

    private void dataUsers(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Address target = address(value);
        Set<Address> users = new HashSet<Address>();
        println("");
        println("DATA USERS role=" + role + " target=" + target);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function user = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " function=" +
                (user == null ? "<none>" : user.getEntryPoint() + " " +
                    user.getName(true)));
            if (user != null) {
                users.add(user.getEntryPoint());
            }
        }
        for (Address entry : users) {
            decompile(
                decompiler,
                currentProgram.getFunctionManager().getFunctionAt(entry),
                "user of " + role,
                seen
            );
        }
    }

    private void vtable(
            DecompInterface decompiler,
            long value,
            int entries,
            String role,
            Set<Address> seen) throws Exception {
        Address table = address(value);
        println("");
        println("VTABLE role=" + role + " address=" + table);
        for (int index = 0; index < entries; ++index) {
            long pointer = Integer.toUnsignedLong(getInt(table.add(index * 4L)));
            Function method = functionAtOrContaining(pointer);
            println("  index=" + index + " pointer=" + address(pointer) +
                " function=" + (method == null ? "<none>" :
                    method.getEntryPoint() + " " + method.getName(true)));
            decompile(decompiler, method, role + " vtable index " + index, seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Pure Land render-history report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] targets = {
            0x0040fba0L, // Spirit Strike activation
            0x00410940L, // Spirit Strike validator
            0x0041be60L, // CSceneManager::SetMotionBlur
            0x005ddfb0L, // motion-blur renderer constructor
            0x005dd9d0L, // render-target state update/restore helper
            0x005ddab0L, // render-target stack push/draw/pop helper
            0x005ddd00L, // render-target stack pop helper
            0x005f65a0L, // renderable-to-target caller A
            0x005f6680L, // renderable-to-target caller B
            0x005dce30L, // frame render start
            0x005dd540L, // frame render end
            0x005dfad0L, // stalled render critical-section helper
            0x005eae00L, // stalled helper caller A
            0x005eaf10L, // stalled helper caller B
            0x005f6c70L, // motion-blur history resource factory
            0x005f63d0L, // motion-blur history resource wrapper constructor
            0x005d94a0L, // render-target backing resource constructor
            0x005d9500L, // render-target backing resource cleanup
            0x005ea450L, // motion-blur history composite helper
            0x005e9fd0L, // screenshot history copy helper
            0x005da9b0L, // screenshot presentation helper
            0x005d5520L, // post-render callback setup helper
            0x005d55a0L, // motion-blur callback setup helper
            0x0040a5b0L, // world render pass
            0x0068d3f0L  // main frame renderer
        };
        String[] roles = {
            "Spirit Strike activation",
            "Spirit Strike validator",
            "CSceneManager::SetMotionBlur",
            "motion-blur renderer constructor",
            "render-target state helper",
            "render-target scoped draw",
            "render-target stack pop",
            "renderable-to-target caller A",
            "renderable-to-target caller B",
            "frame render start",
            "frame render end",
            "stalled render critical-section helper",
            "stalled render helper caller A",
            "stalled render helper caller B",
            "motion-blur history resource factory",
            "motion-blur history resource wrapper constructor",
            "render-target backing resource constructor",
            "render-target backing resource cleanup",
            "motion-blur history composite helper",
            "screenshot history copy helper",
            "screenshot presentation helper",
            "post-render callback setup helper",
            "motion-blur callback setup helper",
            "world render pass",
            "main frame renderer"
        };
        for (int index = 0; index < targets.length; ++index) {
            Function function = functionAtOrContaining(targets[index]);
            decompile(decompiler, function, roles[index], seen);
            callers(
                decompiler,
                function,
                roles[index],
                seen,
                index == 2 || index == 5 || index == 6
            );
        }

        vtable(decompiler, 0x006ca30cL, 8,
            "CSpiritStrikeManager primary", seen);
        vtable(decompiler, 0x006ca330L, 1,
            "CSpiritStrikeManager PtrObj", seen);
        vtable(decompiler, 0x006ca338L, 1,
            "CSpiritStrikeManager singleton", seen);

        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String lower = symbol.getName(true).toLowerCase();
            if ((lower.contains("cd3dmotionblurposteffect::vftable") ||
                    lower.contains("cscreenshotpostrendercallback::vftable")) &&
                    !lower.contains("meta_ptr")) {
                vtable(
                    decompiler,
                    symbol.getAddress().getUnsignedOffset(),
                    8,
                    symbol.getName(true),
                    seen
                );
            }
        }

        dataUsers(decompiler, 0x00808d30L, "Spirit Strike manager global", seen);
        dataUsers(decompiler, 0x00723e74L, "render-target stack pointer", seen);
        dataUsers(decompiler, 0x00804838L, "render-target stack base", seen);
        dataUsers(decompiler, 0x007c31dcL, "D3D9 device global", seen);
        dataUsers(decompiler, 0x00804bc0L,
            "Moon Wolf stalled render critical section", seen);
        decompiler.dispose();
    }
}
