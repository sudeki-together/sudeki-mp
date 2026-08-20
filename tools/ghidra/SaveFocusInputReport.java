// Read-only report for the native save-page focus/navigation path.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class SaveFocusInputReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long v) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(v); }
  private void dump(DecompInterface d, long v, String role) {
    Function f = currentProgram.getFunctionManager().getFunctionAt(a(v));
    println("\n===== " + role + " " + a(v) + " =====");
    if (f == null) { println("missing=true"); return; }
    DecompileResults r = d.decompileFunction(f, 240, monitor);
    println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : r.getErrorMessage());
  }
  private void callers(long v, String role) {
    println("\nCALLERS " + role + " " + a(v));
    ReferenceIterator it = currentProgram.getReferenceManager().getReferencesTo(a(v));
    int n = 0;
    while (it.hasNext() && !monitor.isCancelled()) {
      Reference r = it.next();
      if (!r.getReferenceType().isCall()) continue;
      Function f = currentProgram.getFunctionManager().getFunctionContaining(r.getFromAddress());
      println("callsite=" + r.getFromAddress() + " owner=" + (f == null ? "<none>" : f.getEntryPoint() + " " + f.getName(true)));
      if (++n >= 80) { println("callers_truncated=true"); break; }
    }
    println("callers=" + n);
  }
  @Override public void run() throws Exception {
    if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d = new DecompInterface(); d.openProgram(currentProgram);
    long[] targets = {
      0x004898a0L, 0x0048d970L, 0x0048a590L, 0x0048a900L, 0x0048c090L,
      0x0048c160L, 0x0048c710L, 0x0055e120L, 0x0055e190L, 0x0055e2d0L,
      0x0055e750L, 0x0068aeb0L
    };
    String[] roles = {
      "save_page_dispatcher", "save_page_ui_input", "save_page_ui_entry", "save_page_input",
      "save_page_input_alt", "save_entry_owner", "save_entry_update", "focus_prev", "focus_next",
      "focus_apply", "focus_reset", "ui_event_bridge"
    };
    for (int i = 0; i < targets.length; ++i) { dump(d, targets[i], roles[i]); callers(targets[i], roles[i]); }
    d.dispose();
  }
}
