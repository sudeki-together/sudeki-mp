// Read-only exact-build trace of the world-map UI: the only shipped path
// known to declare all four party portrait resources in one UI configuration.
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

public class WorldMapPortraitReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private void dump(DecompInterface d, Function f, String role, Set<Address> seen) {
    if (f == null || !seen.add(f.getEntryPoint())) return;
    println("\n===== " + role + " " + f.getEntryPoint() + " " + f.getName(true) + " =====");
    DecompileResults r = d.decompileFunction(f, 120, monitor);
    println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : r.getErrorMessage());
  }
  private void refs(DecompInterface d, Address target, String role, Set<Address> seen) {
    println("\n===== REFERENCES " + role + " " + target + " =====");
    ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(target);
    int n = 0;
    while (it.hasNext() && !monitor.isCancelled()) {
      Reference r = it.next();
      Function f = currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());
      println("ref=" + r.getFromAddress() + " type=" + r.getReferenceType() + " owner=" + (f == null ? "<none>" : f.getEntryPoint()));
      dump(d, f, role + " owner", seen);
      if (++n >= 24) { println("refs_truncated=true"); break; }
    }
    println("refs=" + n);
  }
  private void findString(DecompInterface d, String needle, Set<Address> seen) {
    DataIterator all = currentProgram.getListing().getDefinedData(true);
    int matches = 0;
    while (all.hasNext() && !monitor.isCancelled()) {
      Data data = all.next();
      if (!data.hasStringValue()) continue;
      String value = data.getDefaultValueRepresentation();
      if (value == null || !value.contains(needle)) continue;
      println("\nSTRING needle=" + needle + " address=" + data.getAddress() + " value=" + value);
      refs(d, data.getAddress(), needle, seen);
      if (++matches >= 6) break;
    }
    println("STRING_MATCHES needle=" + needle + " count=" + matches);
  }
  @Override public void run() throws Exception {
    if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram); Set<Address> seen = new HashSet<Address>();
    findString(d, "UIICONGROUPMAPMENUENG.UIDATA", seen);
    findString(d, ".?AVUILayerMapMenu@@", seen);
    findString(d, "ui_map.dat", seen);
    findString(d, "SUI_Map_pc", seen);
    d.dispose();
  }
}
