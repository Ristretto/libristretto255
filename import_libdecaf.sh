#!/bin/bash -e
# Import Ristretto implementation from ed448goldilocks (a.k.a. libdecaf),
# filtering commits to just the files we care about.

# URL of the ed448-goldilocks git repository
REPO=git://git.code.sf.net/p/ed448goldilocks/code

# Commit we're importing from (to make this script reproducible)
COMMIT=03977eba48da31dd071a0973aef373ad0441b990

# Files to filter from the upstream repository
export FILES="
    LICENSE.txt
    Makefile
    src/generator/curve_data.py
    src/generator/template.py
    src/include/arch_32/arch_intrinsics.h
    src/include/arch_arm_32/arch_intrinsics.h
    src/include/arch_neon/arch_intrinsics.h
    src/include/arch_ref64/arch_intrinsics.h
    src/include/arch_x86_64/arch_intrinsics.h
    src/include/constant_time.h
    src/include/field.h
    src/include/portable_endian.h
    src/include/word.h
    src/p25519/arch_ref64/f_impl.h
    src/p25519/arch_ref64/f_impl.c
    src/p25519/arch_x86_64/f_impl.h
    src/p25519/arch_x86_64/f_impl.c
    src/p25519/arch_32/f_impl.h
    src/p25519/arch_32/f_impl.c
    src/p25519/f_arithmetic.c
    src/per_curve/decaf.tmpl.c
    src/per_curve/decaf_gen_tables.tmpl.c
    src/per_curve/elligator.tmpl.c
    src/per_curve/eddsa.tmpl.h
    src/per_curve/scalar.tmpl.c
    src/per_curve/point.tmpl.h
    src/per_field/f_field.tmpl.h
    src/per_field/f_generic.tmpl.c
    src/public_include/decaf.tmpl.h
    src/public_include/decaf/common.h
    src/utils.c"

echo "*** Importing Ristretto255 code from ed448goldilocks"
echo "Repo:   $REPO"
echo "Commit: $COMMIT"
echo "Files:  $(echo $FILES | xargs)"
echo

# Clone the upstream 'ed448goldilocks' repo from SourceForge
git clone $REPO ed448goldilocks

cd ed448goldilocks

# Reset to the specified commit
git reset --hard $COMMIT

# Filter files relevant to Ristretto255
git filter-branch \
    --prune-empty \
    --index-filter 'git read-tree --empty; git reset $GIT_COMMIT -- $FILES' \
    -- --all -- $FILES

cd ..

# Create the commit message for the merge
cat << EOF > /tmp/COMMIT_MSG.txt
Merge 'ed448goldilocks' repo using the './import_libdecaf.sh' script

Imports a subset of the 'ed448goldilocks' repo as the basis of libristretto255.

This commit merges the 'ed448goldilocks' repository (a.k.a. libdecaf) after a
'git filter-branch' operation was automatically performed by the
'./import_libdecaf.sh' script.

- Upstream Repo URL:  $REPO
- Original Commit ID: $COMMIT
- Filtered Files:     $FILES
EOF

# Display the commit message
echo "*** Merging filtered ed448goldilocks code with commit message:"
cat /tmp/COMMIT_MSG.txt
echo

# Merge the filtered repository into this one
git remote add ed448goldilocks-local ed448goldilocks
git fetch ed448goldilocks-local
git merge ed448goldilocks-local/master --allow-unrelated-histories -m $(cat /tmp/COMMIT_MSG.txt)
