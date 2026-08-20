// Reports the exact native UIElementBar construction/resource path used by
// the title Options submenu. Read-only and hash-gated.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class TitleNativeRowReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private DecompInterface decompiler;

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void reportFunction(String label, long value) throws Exception {
        Function function = getFunctionAt(address(value));
        println("\n===== " + label + " " + address(value) + " =====");
        if (function == null) {
            println("missing=true");
            return;
        }
        DecompileResults results = decompiler.decompileFunction(
            function, 120, monitor);
        println("function=" + function.getName());
        println(results != null && results.decompileCompleted() ?
            results.getDecompiledFunction().getC() : "decompile_failed=true");
    }

    private void reportSymbols(String fragment) {
        println("\n===== symbols containing " + fragment + " =====");
        SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).toLowerCase().contains(fragment.toLowerCase())) {
                println("symbol=" + symbol.getName(true) + " address=" +
                    symbol.getAddress());
            }
        }
    }

    private void reportReferences(String label, long value) {
        println("\n===== references to " + label + " " + address(value) + " =====");
        Reference[] references = getReferencesTo(address(value));
        int count = 0;
        for (Reference reference : references) {
            if (monitor.isCancelled()) {
                break;
            }
            Function owner = getFunctionContaining(reference.getFromAddress());
            println("from=" + reference.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " + owner.getName()));
            count++;
        }
        println("count=" + count);
    }

    private void reportVtable(String label, long value, int slots) throws Exception {
        println("\n===== " + label + " vtable " + address(value) + " =====");
        for (int slot = 0; slot < slots; ++slot) {
            long target = Integer.toUnsignedLong(getInt(address(value + slot * 4L)));
            println(String.format("slot=0x%02X target=%s", slot * 4,
                address(target)));
            if (slot < 6 || slot == 0x14 / 4 || slot == 0x18 / 4 ||
                slot == 0x1c / 4 || slot == 0x20 / 4) {
                reportFunction(label + " slot 0x" + Integer.toHexString(slot * 4),
                    target);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        println("SudekiMP title native-row report");
        println("SHA256=" + actual);
        reportFunction("Options page row initializer", 0x0051d4d0L);
        reportFunction("UIBlock/resource preparation", 0x004a7110L);
        reportFunction("UIElement load/register", 0x00581f90L);
        reportFunction("UIElement renderer-array allocation", 0x00582430L);
        reportFunction("UI renderer-instance allocation", 0x00559110L);
        reportFunction("UI renderer-instance activation", 0x00558fb0L);
        reportFunction("UI state request", 0x00520260L);
        reportFunction("Options page constructor", 0x0051a7b0L);
        reportFunction("Options page destructor", 0x0051c050L);
        reportFunction("UIElementBar constructor", 0x00581ec0L);
        reportFunction("UIElementBar destructor", 0x00581f40L);
        reportFunction("UIElementBar visibility application", 0x00582350L);
        reportFunction("UIElementBar aggregate-state update", 0x0055c2d0L);
        reportFunction("Options page input/state handler", 0x0051dc30L);
        reportFunction("Options page frame update", 0x0051da40L);
        reportFunction("Animated title row constructor", 0x0051fb40L);
        reportFunction("Animated title row resource binding", 0x0051fe60L);
        reportFunction("Animated title row frame update", 0x0051ff90L);
        reportFunction("Animated title row state queue", 0x00520260L);
        reportFunction("Animated title row state begin", 0x005202c0L);
        reportFunction("Animated title row animation map", 0x00520370L);
        reportFunction("Animated title row listener bind", 0x005204f0L);
        reportFunction("Animated title row state complete", 0x005205a0L);
        reportFunction("Animated title row transition weight", 0x005206c0L);
        reportFunction("CUIScene resource attach", 0x0040af20L);
        reportFunction("UI animated mesh resource visibility", 0x00411600L);
        reportFunction("UI animated mesh resource reference", 0x00411610L);
        reportFunction("Anim renderer component getter 18", 0x0061bc80L);
        reportFunction("Anim renderer component getter 1c", 0x0061bc90L);
        reportFunction("Anim renderer component getter 20", 0x0061bd20L);
        reportFunction("Anim renderer component getter 24", 0x0061bd40L);
        reportFunction("Anim renderer component getter 28", 0x0061bce0L);
        reportFunction("Anim renderer submodel helper 5c", 0x0061bb70L);
        reportFunction("Anim renderer submodel helper 60", 0x0061bb90L);
        reportFunction("Anim renderer submodel helper 64", 0x0061bbb0L);
        reportFunction("Anim renderer submodel helper 68", 0x0061bbd0L);
        reportFunction("Anim renderer submodel helper 6c", 0x0061bba0L);
        reportFunction("Anim renderer submodel helper 70", 0x0061bb80L);
        reportFunction("Anim renderer submodel count 88", 0x0061bb50L);
        reportFunction("Anim renderer submodel count 8c", 0x0061bb60L);
        reportFunction("Native name-to-index helper", 0x004c5ff0L);
        reportFunction("Animated title row reset helper", 0x0051f8c0L);
        reportFunction("Front-end controller constructor", 0x0049f110L);
        reportFunction("Front-end title row refresh", 0x004a16f0L);
        reportFunction("Front-end title page setup", 0x004a27e0L);
        reportReferences("Animated title row constructor", 0x0051fb40L);
        reportReferences("Animated title row reset helper", 0x0051f8c0L);
        reportFunction("Generic row factory A", 0x004a6d20L);
        reportFunction("Generic row factory B", 0x004aa170L);
        reportFunction("Generic row factory C", 0x004adee0L);
        reportSymbols("UIElementBar");
        reportSymbols("UIAnimatedMesh");
        reportSymbols("OPTIONBAR0SG");
        reportVtable("UIElementBar", 0x006d9024L, 16);
        reportVtable("UIAnimatedMesh", 0x006d1da8L, 20);
        reportReferences("UIElementBar vtable", 0x006d9024L);
        reportReferences("UIElement load/register", 0x00581f90L);
        reportReferences("UIElementBar visibility application", 0x00582350L);
        reportReferences("UIElementBar aggregate-state update", 0x0055c2d0L);
        decompiler.dispose();
    }
}
