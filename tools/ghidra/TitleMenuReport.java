// Reports the native title/front-end menu string owners and their callers.
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

public class TitleMenuReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private void decompile(DecompInterface decompiler, Function function,
            String role, Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println("\n===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private boolean isTitleString(String value) {
        String s = value.toLowerCase();
        return s.contains("new game") || s.contains("load game") ||
            s.equals("options") || s.equals("continue") ||
            s.equals("credits") || s.equals("quitgame") ||
            s.contains("quit to title") || s.contains("exit to windows") ||
            s.contains("title screen") || s.contains("front end");
    }

    private void reportString(Data data) {
        String value = data.getDefaultValueRepresentation();
        println("\nSTRING address=" + data.getAddress() + " value=" + value);
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(data.getAddress());
        int count = 0;
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            if (count++ >= 32) {
                println("  references_truncated=true");
                break;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("  reference=" + ref.getFromAddress() + " type=" +
                ref.getReferenceType() + " function=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
        }
        if (count == 0) {
            println("  references=0");
        }
    }

    private void reportAddress(long value, String label) {
        Address address = currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
        println("\nKNOWN_UTF16_STRING address=" + address + " label=" + label);
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(address);
        int count = 0;
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("  reference=" + ref.getFromAddress() + " type=" +
                ref.getReferenceType() + " function=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            count++;
        }
        println("  references=" + count);
    }

    private void reportFunctionCallers(long value, String label) {
        Address target = currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
        println("\nFUNCTION_CALLERS address=" + target + " label=" + label);
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        int count = 0;
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            if (!ref.getReferenceType().isCall()) {
                continue;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("  callsite=" + ref.getFromAddress() + " caller=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            count++;
        }
        println("  callers=" + count);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP native title/menu report");
        println("SHA256=" + actual);
        println("ImageBase=" + currentProgram.getImageBase());
        reportFunctionCallers(0x004a1950L, "native title/menu string dispatcher");
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                currentProgram.getAddressFactory().getDefaultAddressSpace()
                    .getAddress(0x004a1950L)),
            "native title/menu string dispatcher", new HashSet<Address>());
        reportAddress(0x006cb140L, "Options");
        reportAddress(0x006cb164L, "Continue");
        reportAddress(0x006cb178L, "Credits");
        reportAddress(0x006cb188L, "QuitGame");
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                currentProgram.getAddressFactory().getDefaultAddressSpace()
                    .getAddress(0x004a0f40L)),
            "title/menu setup caller", new HashSet<Address>());
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                currentProgram.getAddressFactory().getDefaultAddressSpace()
                    .getAddress(0x004a0060L)),
            "front-end setup caller", new HashSet<Address>());
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                currentProgram.getAddressFactory().getDefaultAddressSpace()
                    .getAddress(0x004a0360L)),
            "front-end transition caller", new HashSet<Address>());
        DataIterator data = currentProgram.getListing().getDefinedData(true);
        int matches = 0;
        while (data.hasNext() && !monitor.isCancelled()) {
            Data item = data.next();
            if (!item.hasStringValue()) {
                continue;
            }
            String value = item.getDefaultValueRepresentation();
            if (isTitleString(value)) {
                reportString(item);
                matches++;
                if (matches >= 64) {
                    println("\nTITLE_STRING_MATCHES_TRUNCATED=true");
                    break;
                }
            }
        }
        println("\nDEFINED_STRING_MATCHES=" + matches +
            " (known UTF-16 labels are reported by address above)");
        decompiler.dispose();
    }
}
