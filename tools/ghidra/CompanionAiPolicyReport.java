// Reports native party stance UI, request builders and leader targeting.
// Read-only; inspect only the supported executable. Extra arguments are VAs.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.DataIterator;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import java.util.HashSet;
import java.util.Set;

public class CompanionAiPolicyReport extends GhidraScript {
    private final Set<Address> seen = new HashSet<>();
    private DecompInterface decompiler;
    private Address va(long value) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value); }
    private void report(Function function) {
        if (function == null || !seen.add(function.getEntryPoint())) return;
        println("\nFUNCTION " + function.getEntryPoint() + " " + function.getName(true));
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println(result.decompileCompleted() ? result.getDecompiledFunction().getC() : result.getErrorMessage());
    }
    private void references(Address address) {
        ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(address);
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            Function owner = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
            println("REF " + address + " from=" + ref.getFromAddress() + " type=" + ref.getReferenceType());
            report(owner);
        }
    }
    @Override protected void run() throws Exception {
        if (!"8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94".equalsIgnoreCase(currentProgram.getExecutableSHA256()) ||
            currentProgram.getImageBase().getOffset() != 0x400000L) throw new Exception("Unexpected executable identity");
        println("Companion AI policy report SHA256=" + currentProgram.getExecutableSHA256());
        decompiler = new DecompInterface();
        try {
            decompiler.openProgram(currentProgram);
            for (String arg : getScriptArgs()) {
                Address target = va(Long.decode(arg));
                report(currentProgram.getFunctionManager().getFunctionAt(target));
                references(target);
            }
            if (getScriptArgs().length != 0) return;
            String[] needles = {"SUI_AIbox_FocusedAttack", "SUI_AIbox_Retreat", "AI Attack Style"};
            DataIterator data = currentProgram.getListing().getDefinedData(true);
            while (data.hasNext() && !monitor.isCancelled()) {
                Data item = data.next();
                if (!item.hasStringValue()) continue;
                String value = item.getDefaultValueRepresentation();
                for (String needle : needles) if (value != null && value.contains(needle)) {
                    println("STRING " + item.getAddress() + " " + value);
                    references(item.getAddress());
                    break;
                }
            }
            long[] functions = {0x5b76c0L,0x5b6c70L,0x5adf60L,0x5ae000L,0x5ae820L,0x5ae940L,
                0x5ae070L,0x5ae1f0L,0x5ae280L,0x5ae330L,0x4ec350L,0x4f6100L};
            for (long value : functions) report(currentProgram.getFunctionManager().getFunctionAt(va(value)));
        } finally { decompiler.dispose(); }
    }
}
