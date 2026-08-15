// Focused read-only report for the viewport HUD portrait binding path.
// Refuses to inspect any executable other than the supported GOG build.
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

public class HudPortraitBindingReport extends GhidraScript {
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
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
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
            Set<Address> seen) {
        if (function == null) return;
        println("CALLERS role=" + role + " function=" +
            function.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) continue;
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            decompile(decompiler, caller, "caller of " + role, seen);
        }
    }

    private void references(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Address target = address(value);
        println("");
        println("REFERENCES role=" + role + " address=" + target);
        ReferenceIterator iterator = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Reference reference = iterator.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  reference=" + reference.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            decompile(decompiler, owner, "reference to " + role, seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP HUD portrait binding report");
        println("SHA256=" + actualSha256);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] functions = {
            0x00581930L, 0x0051fb40L, 0x004a8df0L,
            0x004a8e80L, 0x00581ec0L, 0x004a9060L,
            0x004a95e0L, 0x004ab4c0L, 0x004aa170L,
            0x0055be70L, 0x00581f90L, 0x004a8fc0L,
            0x0055c020L, 0x0055c070L, 0x0055c0e0L,
            0x0055c190L, 0x0055c230L, 0x0055c2c0L,
            0x0055c270L, 0x0055c330L, 0x004a9d40L,
            0x00559110L, 0x00559280L
        };
        String[] roles = {
            "UIPortraitGroup bind children", "portrait gizmo factory",
            "portrait gizmo setup helper", "portrait gizmo teardown helper",
            "UI cycle-icon constructor", "UIPortraitGizmo constructor",
            "UIPortraitGizmo update", "nearby portrait method",
            "UIPortraitGizmo bind", "UI element name binding",
            "UI bar binding", "indexed UI binding helper",
            "UIElementCycleIcon vfunc 4", "cycle icon character selector",
            "cycle icon resource assignment", "cycle icon direct shape setter",
            "UIElementCycleIcon vfunc 5", "UIElementCycleIcon secondary vfunc 0",
            "UIElementCycleIcon secondary vfunc 1", "cycle icon state refresh",
            "UIPortraitGizmo party-value refresh",
            "UI element ID lookup", "UI scene node collection"
        };
        for (int index = 0; index < functions.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(functions[index]));
            decompile(decompiler, function, roles[index], seen);
            callers(decompiler, function, roles[index], seen);
        }

        references(decompiler, 0x006c3254L,
            "HUD_%d_Portrait format pointer", seen);
        references(decompiler, 0x006c3258L,
            "HUD_%d_PortraitSG format pointer", seen);
        references(decompiler, 0x006d7aa0L,
            "HUD_%d_Portrait string", seen);
        references(decompiler, 0x006d7a8cL,
            "HUD_%d_PortraitSG string", seen);
        references(decompiler, 0x006cfb0cL,
            "SUI_PORTRAIT_TAL.SQX string", seen);
        references(decompiler, 0x006cfb24L,
            "SUI_PORTRAIT_AILISH.SQX string", seen);
        references(decompiler, 0x006cfb3cL,
            "SUI_PORTRAIT_BUKI.SQX string", seen);
        references(decompiler, 0x006cfb54L,
            "SUI_PORTRAIT_ELCO.SQX string", seen);
        references(decompiler, 0x007ca570L,
            "portrait character resource table", seen);
        println("");
        println("SYMBOLS containing UIElementCycleIcon");
        SymbolIterator symbols = currentProgram.getSymbolTable()
            .getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).contains("UIElementCycleIcon")) {
                println("  symbol=" + symbol.getAddress() + " " +
                    symbol.getName(true));
            }
        }
        decompiler.dispose();
    }
}
