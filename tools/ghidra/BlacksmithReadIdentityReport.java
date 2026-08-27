// Reports exact RTTI/vtable identities needed before the read-only
// Blacksmith adapter invokes native virtual methods. Read-only and hash-gated.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class BlacksmithReadIdentityReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, Address target,
            String role, Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(target);
        if (function == null || !seen.add(target)) return;
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        println("\n===== " + role + " " + target + " " +
            function.getName(true) + " =====");
        if (result.decompileCompleted()) {
            println(result.getDecompiledFunction().getC());
        } else {
            println("Decompiler failed: " + result.getErrorMessage());
        }
    }

    private long unsignedInt(Address location) throws Exception {
        return Integer.toUnsignedLong(getInt(location));
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP Blacksmith read identity report SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        SymbolIterator symbols = currentProgram.getSymbolTable()
            .getAllSymbols(true);
        while (symbols.hasNext() && !monitor.isCancelled()) {
            Symbol symbol = symbols.next();
            String name = symbol.getName(true);
            if (!(name.contains("CItemWeapon") ||
                    name.contains("CItemArmour") ||
                    name.contains("CItem@@") ||
                    name.toLowerCase().contains("locali"))) {
                continue;
            }
            Address at = symbol.getAddress();
            println("symbol " + at + " " + name + " type=" +
                symbol.getSymbolType());
            if (name.contains("vftable") || name.contains("vtable") ||
                    name.contains("??_7")) {
                println("  slot30=" +
                    address(unsignedInt(at.add(0x30))) +
                    " slot34=" + address(unsignedInt(at.add(0x34))));
                decompile(decompiler,
                    address(unsignedInt(at.add(0x30))),
                    name + " slot+30", seen);
                decompile(decompiler,
                    address(unsignedInt(at.add(0x34))),
                    name + " slot+34", seen);
            }
        }
        ReferenceIterator localizationRefs = currentProgram
            .getReferenceManager().getReferencesTo(address(0x00809e0cL));
        while (localizationRefs.hasNext() && !monitor.isCancelled()) {
            Reference reference = localizationRefs.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("localization global ref " + reference.getFromAddress() +
                " type=" + reference.getReferenceType() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint()));
            if (owner != null) {
                decompile(decompiler, owner.getEntryPoint(),
                    "localization global owner", seen);
            }
        }
        decompiler.dispose();
    }
}
