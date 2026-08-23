// Reports Talos-relevant damage, invulnerability, and knockback-session paths.
// Read-only and exact-build gated.
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
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

import java.util.HashSet;
import java.util.Set;

public class TalosDefenseReport extends GhidraScript {
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
        Set<Address> seen
    ) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        println("\n===== " + role + " " + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        DecompileResults results = decompiler.decompileFunction(
            function, 120, monitor);
        println(results.decompileCompleted() ?
            results.getDecompiledFunction().getC() :
            results.getErrorMessage());
    }

    private void reportFunction(
        DecompInterface decompiler,
        long value,
        String role,
        Set<Address> seen
    ) {
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(address(value)),
            role, seen);
    }

    private void reportReferences(
        DecompInterface decompiler,
        long value,
        String role,
        Set<Address> seen
    ) {
        Address target = address(value);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        println("\n===== REFERENCES " + role + " " + target + " =====");
        while (references.hasNext()) {
            Reference reference = references.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("ref=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint()));
            decompile(decompiler, owner, role + " caller", seen);
        }
    }

    private void reportStringOwners(
        DecompInterface decompiler,
        String needle,
        Set<Address> seen
    ) {
        DataIterator iterator = currentProgram.getListing().getDefinedData(true);
        int matches = 0;
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Data data = iterator.next();
            String value;
            ReferenceIterator references;
            if (!data.hasStringValue()) {
                continue;
            }
            value = data.getDefaultValueRepresentation();
            if (value == null || !value.contains(needle)) {
                continue;
            }
            println("\nSTRING needle=" + needle + " address=" +
                data.getAddress() + " value=" + value);
            references = currentProgram.getReferenceManager()
                .getReferencesTo(data.getAddress());
            while (references.hasNext()) {
                Reference reference = references.next();
                Function owner = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("ref=" + reference.getFromAddress() + " type=" +
                    reference.getReferenceType() + " owner=" +
                    (owner == null ? "<none>" : owner.getEntryPoint()));
                decompile(decompiler, owner, needle + " owner", seen);
            }
            ++matches;
        }
        println("STRING_MATCHES needle=" + needle + " count=" + matches);
    }

    private void reportSymbols(String needle) {
        SymbolIterator symbols = currentProgram.getSymbolTable()
            .getAllSymbols(true);
        println("\n===== SYMBOLS " + needle + " =====");
        while (symbols.hasNext()) {
            Symbol symbol = symbols.next();
            if (symbol.getName(true).toLowerCase()
                    .contains(needle.toLowerCase())) {
                println(symbol.getAddress() + " " + symbol.getName(true));
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        println("SudekiMP Talos defense report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        Set<Address> seen = new HashSet<Address>();
        decompiler.openProgram(currentProgram);

        reportStringOwners(decompiler, "Num KnockBacks in Session", seen);
        reportStringOwners(decompiler, "KnockBack Session Length", seen);
        reportStringOwners(decompiler, "Use KnockBack System", seen);
        reportSymbols("CCombat");
        reportSymbols("KnockBack");
        reportSymbols("HitReaction");

        reportFunction(decompiler, 0x004d21d0L,
            "Apply damage to character", seen);
        reportFunction(decompiler, 0x004dab50L,
            "Dispatch damage structure", seen);
        reportFunction(decompiler, 0x00538870L,
            "Collision damage callback", seen);
        reportFunction(decompiler, 0x00408980L,
            "CCharacterArbiter::IsInvulnerable", seen);
        reportFunction(decompiler, 0x004dca10L,
            "CCharacterArbiter::GELSetInvulnerable", seen);
        reportFunction(decompiler, 0x00585530L,
            "DamageStructure constructor", seen);
        reportFunction(decompiler, 0x00585960L,
            "DamageStructure set damage", seen);
        reportFunction(decompiler, 0x00585ba0L,
            "DamageStructure mitigate damage", seen);
        reportFunction(decompiler, 0x00585c40L,
            "DamageStructure store final HP", seen);
        reportFunction(decompiler, 0x00586120L,
            "DamageStructure qualification", seen);
        reportFunction(decompiler, 0x004d20f0L,
            "Hit presentation selector", seen);
        reportFunction(decompiler, 0x004d29d0L,
            "Post-damage reaction", seen);
        reportFunction(decompiler, 0x004d2d80L,
            "Damage reaction session classifier", seen);
        reportFunction(decompiler, 0x004d3f20L,
            "Launch hit reaction animation", seen);
        reportFunction(decompiler, 0x004d31c0L,
            "Death follow-up", seen);
        reportFunction(decompiler, 0x004d3ba0L,
            "Damage notification", seen);

        reportReferences(decompiler, 0x004d21d0L,
            "Apply damage to character", seen);
        reportReferences(decompiler, 0x00408980L,
            "CCharacterArbiter::IsInvulnerable", seen);
        reportReferences(decompiler, 0x004dca10L,
            "CCharacterArbiter::GELSetInvulnerable", seen);

        decompiler.dispose();
    }
}
