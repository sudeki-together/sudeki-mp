// Read-only exact-build report for cD3DTexture's resident job layout.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import java.util.HashSet;
import java.util.Set;

public class D3DTextureObjectLayoutReport extends GhidraScript {
  private static final String SHA = "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
  private Address a(long value) { return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value); }
  private void dump(DecompInterface d, long va, String name, Set<Address> seen) {
    Function f = currentProgram.getFunctionManager().getFunctionAt(a(va));
    println("\n===== " + name + " " + a(va) + " =====");
    if (f == null || !seen.add(f.getEntryPoint())) { println("missing_or_seen=true"); return; }
    DecompileResults r = d.decompileFunction(f, 120, monitor);
    println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : r.getErrorMessage());
  }
  @Override public void run() throws Exception {
    if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) throw new Exception("unexpected image");
    DecompInterface d = new DecompInterface();
    d.openProgram(currentProgram);
    Set<Address> seen = new HashSet<Address>();
    long[] targets = {
      0x005d6c80L, 0x005d9630L, 0x005d9800L, 0x005d9af0L,
      0x005d71d0L, 0x005d7230L, 0x005d6dd0L, 0x005d8b50L,
      0x005f45c0L, 0x005f4690L, 0x005f4760L, 0x005eac20L, 0x005eae20L,
      0x005d92e0L, 0x005e3620L, 0x005d8d70L, 0x005d91b0L,
      0x005d7620L, 0x005da050L, 0x005d9250L, 0x005d8210L,
      0x005d6a90L, 0x005d8190L, 0x005d7450L, 0x005d9580L
    };
    String[] names = {
      "texture_job_constructor", "texture_job_release", "texture_wrapper_submit",
      "resident_texture_release", "texture_backend_prepare", "texture_backend_finalize",
      "texture_backend_cancel", "texture_backend_upload", "texture_job_ref_release",
      "texture_job_ref_acquire", "texture_job_ref_finish", "texture_worker_decode", "texture_worker_upload",
      "native_texture_wrapper_create", "resource_stream_open", "native_texture_enqueue",
      "texture_duplicate_lookup", "resource_name_lookup", "resource_id_lookup", "resource_name_fallback",
      "queued_texture_create", "non_d3d_texture_wrapper_create", "texture_aux_data", "texture_variant_list", "texture_prepare"
    };
    for (int i = 0; i < targets.length; ++i) dump(d, targets[i], names[i], seen);
    d.dispose();
  }
}
