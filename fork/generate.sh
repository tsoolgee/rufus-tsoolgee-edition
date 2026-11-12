#!/usr/bin/env bash
# Rufus tsoolgee - re-apply the tsoolgee changes on top of an upstream Rufus tag.
#
#   fork/generate.sh <upstream-tag> [source-commit]
#
# Needs the upstream tags under refs/upstream-tags/* :
#   git fetch --no-tags https://github.com/pbatard/rufus.git '+refs/tags/*:refs/upstream-tags/*'
#
# The tsoolgee changes are whatever the source commit (default: HEAD, i.e. the 'tsoolgee'
# branch) changed compared to the upstream tag it is based on (fork/BASE). They are
# cherry-picked onto <upstream-tag> (3-way merge, line-ending changes ignored - upstream
# switches files between CRLF and LF from time to time). The fork's own files (fork/, our
# workflows, README.md) are always taken from the source commit, upstream's workflows are
# dropped and upstream's README becomes README.rufus.md. The result is committed on a
# detached HEAD and tagged <upstream-tag> locally. Exits non-zero on real code conflicts.
#
# NB: copy this script somewhere else before running it from a checkout that it modifies.
set -euo pipefail

TAG="$1"
SRC="$(git rev-parse "${2:-HEAD}^{commit}")"
BASE="$(git show "$SRC:fork/BASE" | tr -d '\r\n ')"
NAME=tsoolgee
EMAIL=291792265+tsoolgee@users.noreply.github.com

git rev-parse -q --verify "refs/upstream-tags/$TAG^{commit}" >/dev/null || { echo "::error::Unknown upstream tag $TAG"; exit 1; }
git rev-parse -q --verify "refs/upstream-tags/$BASE^{commit}" >/dev/null || { echo "::error::Unknown base tag $BASE"; exit 1; }

echo "Applying tsoolgee changes ($BASE -> ${SRC:0:12}) onto upstream $TAG"
# A single commit holding all the tsoolgee changes, on top of the base tag
SYN=$(git -c user.name=$NAME -c user.email=$EMAIL commit-tree "$SRC^{tree}" -p "refs/upstream-tags/$BASE^{commit}" -m "tsoolgee changes")

git checkout -q --detach "refs/upstream-tags/$TAG"
git -c user.name=$NAME -c user.email=$EMAIL -c merge.conflictstyle=merge \
	cherry-pick --no-commit -X ignore-cr-at-eol "$SYN" >/dev/null 2>&1 || true

# Only our own workflows: upstream's need secrets we don't have (and must not run here)
git rm -r -q -f --ignore-unmatch .github/workflows
rm -rf .github/workflows
# The fork's own files always come from the source commit
git checkout "$SRC" -- fork README.md
git ls-tree -r --name-only "$SRC" .github/workflows | grep '/tsoolgee-' | xargs -r git checkout "$SRC" --
# Upstream's README, as of this tag
git show "refs/upstream-tags/$TAG:README.md" > README.rufus.md
git add README.rufus.md
printf '%s\n' "$TAG" > fork/BASE
git add fork/BASE

CONFLICTS=$(git diff --name-only --diff-filter=U)
if [ -n "$CONFLICTS" ]; then
	echo "::error::The tsoolgee changes do not apply cleanly on upstream $TAG - manual update needed in:"
	echo "$CONFLICTS"
	git diff --diff-filter=U | head -200
	exit 1
fi
if git grep -n -E '^(<<<<<<<|>>>>>>>) ' -- src res >/dev/null; then
	echo "::error::Conflict markers left in the sources"; git grep -n -E '^(<<<<<<<|>>>>>>>) ' -- src res; exit 1
fi

git -c user.name=$NAME -c user.email=$EMAIL \
	commit -q -m "Rufus tsoolgee ${TAG#v}" -m "tsoolgee changes re-applied on top of upstream Rufus $TAG."
git tag -f "$TAG" >/dev/null
echo "Generated $(git rev-parse --short HEAD) for $TAG"
