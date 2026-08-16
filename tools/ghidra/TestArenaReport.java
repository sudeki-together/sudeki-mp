// Reports Sudeki's shipped test-room and native zone-loading entry points.
// Read-only: refuses to inspect any executable other than the supported GOG build.
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

public class TestArenaReport extends GhidraScript {
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

    private void reportStrings(
            DecompInterface decompiler,
            Set<Address> seen) {
        DataIterator dataIterator = currentProgram.getListing().getDefinedData(true);
        while (dataIterator.hasNext() && !monitor.isCancelled()) {
            Data data = dataIterator.next();
            if (!data.hasStringValue()) {
                continue;
            }
            String value = data.getDefaultValueRepresentation();
            String lower = value.toLowerCase();
            if (!lower.contains("testroom") &&
                !lower.contains("levelselect") &&
                !lower.contains("training_dummy") &&
                !lower.equals("\".zone\"") &&
                !lower.equals("\".zoneinfo\"")) {
                continue;
            }
            println("");
            println("STRING address=" + data.getAddress() + " value=" + value);
            ReferenceIterator references = currentProgram.getReferenceManager()
                .getReferencesTo(data.getAddress());
            while (references.hasNext() && !monitor.isCancelled()) {
                Reference reference = references.next();
                Function function = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("  reference=" + reference.getFromAddress() + " function=" +
                    (function == null ? "<none>" : function.getEntryPoint() +
                        " " + function.getName(true)));
                decompile(decompiler, function, "test-arena string owner", seen);
            }
        }
    }

    private void reportDataReferences(
            DecompInterface decompiler,
            Address dataAddress,
            String role,
            Set<Address> seen) {
        println("");
        println("DATA REFERENCES role=" + role + " address=" + dataAddress);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(dataAddress);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  reference=" + reference.getFromAddress() + " function=" +
                (function == null ? "<none>" : function.getEntryPoint() + " " +
                    function.getName(true)));
            decompile(decompiler, function, "reference to " + role, seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP test-arena report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        long[] targets = {
            0x00407910L, // SetZoneNOW(char const *)
            0x00407970L, // EnterZone(char const *)
            0x00407990L, // SwitchZoneNOW(char const *)
            0x004079d0L, // CWorld__OnTestLevel()
            0x00407b80L, // LoadZone(char const *)
            0x00406380L, // CWorld::SwitchMainZone(char const *)
            0x004a0eb0L, // FrontEndSetLevelSelectXml(char const *)
            0x00405d00L, // CWorld::LockActiveZone(char const *)
            0x005051f0L, // DoOneLevelTest()
            0x00451310L, // IsDevMode()
            0x004a2900L, // SetDevMode(bool), retail stub
            0x004fda30L, // IsZoneFile(char const *)
            0x00505190L  // StripZoneExt(char const *)
        };
        String[] roles = {
            "SetZoneNOW(char const *)",
            "EnterZone(char const *)",
            "SwitchZoneNOW(char const *)",
            "CWorld__OnTestLevel()",
            "LoadZone(char const *)",
            "CWorld::SwitchMainZone(char const *)",
            "FrontEndSetLevelSelectXml(char const *)",
            "CWorld::LockActiveZone(char const *)",
            "DoOneLevelTest()",
            "IsDevMode()",
            "SetDevMode(bool)",
            "IsZoneFile(char const *)",
            "StripZoneExt(char const *)"
        };
        for (int index = 0; index < targets.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(targets[index]));
            decompile(decompiler, function, roles[index], seen);
            reportCallers(decompiler, function, roles[index], seen);
        }
        reportDataReferences(
            decompiler,
            address(0x00808d88L),
            "DoOneLevelTest startup gate",
            seen);
        reportStrings(decompiler, seen);
        decompiler.dispose();
    }
}
