#!/usr/bin/env bash
# Idempotent GitHub Release helpers with retries, used by release.yml and mobile.yml.
#
#   release.sh ensure <tag> <sha> <title> <notes-file> [--prerelease] [--not-latest]
#       Creates the tag (if missing) and the release (if missing); otherwise
#       updates the existing release's title and notes. Safe to re-run.
#   release.sh upload <tag> <file>...
#       Attaches files to the release, replacing files with the same name.
#   release.sh notes <tag> <notes-file>
#       Replaces the release notes.
#   release.sh download <tag> <dir>
#       Downloads every file attached to the release (nothing if there is none).
#
# Requires GH_TOKEN (contents: write) and GH_REPO (owner/repo) in the environment.
# GitHub's API occasionally answers 5xx for a while; every call is retried with
# backoff (about 5 minutes in total) instead of failing the whole release.
set -euo pipefail

: "${GH_TOKEN:?GH_TOKEN is required}"
: "${GH_REPO:?GH_REPO is required}"

ATTEMPTS=${RELEASE_ATTEMPTS:-7}

retry() {
  local attempt=1 delay=5
  while true; do
    if "$@"; then return 0; fi
    if (( attempt >= ATTEMPTS )); then
      echo "::error::Giving up after $attempt attempts: $*" >&2
      return 1
    fi
    echo "::warning::Attempt $attempt failed, retrying in ${delay}s: $*" >&2
    sleep "$delay"
    attempt=$((attempt + 1))
    delay=$((delay * 2 > 60 ? 60 : delay * 2))
  done
}

tag_exists() {
  gh api "repos/$GH_REPO/git/ref/tags/$1" --silent 2>/dev/null
}

create_tag() {
  local tag=$1 sha=$2
  # Another job may have created it meanwhile.
  tag_exists "$tag" && return 0
  gh api -X POST "repos/$GH_REPO/git/refs" -f "ref=refs/tags/$tag" -f "sha=$sha" --silent
}

release_exists() {
  gh release view "$1" --json tagName >/dev/null 2>&1
}

ensure_release() {
  local tag=$1 sha=$2 title=$3 notes=$4
  shift 4
  local prerelease=false latest=true
  for option in "$@"; do
    case "$option" in
      --prerelease) prerelease=true ;;
      --not-latest) latest=false ;;
      *) echo "Unknown option $option" >&2; return 2 ;;
    esac
  done

  if ! tag_exists "$tag"; then
    retry create_tag "$tag" "$sha"
  fi

  if release_exists "$tag"; then
    echo "Release $tag exists; updating title and notes"
    retry gh release edit "$tag" --title "$title" --notes-file "$notes" >/dev/null
    return 0
  fi

  local args=("$tag" --verify-tag --title "$title" --notes-file "$notes")
  if [[ "$prerelease" == true ]]; then args+=(--prerelease); fi
  if [[ "$latest" == true && "$prerelease" == false ]]; then args+=(--latest); else args+=(--latest=false); fi
  create() {
    # A failed attempt may still have created the release on GitHub's side.
    release_exists "$tag" || gh release create "${args[@]}" >/dev/null
  }
  retry create
  echo "Release $tag created"
}

upload() {
  local tag=$1
  shift
  local file
  for file in "$@"; do
    [[ -f "$file" ]] || continue
    echo "Uploading $(basename "$file")"
    retry gh release upload "$tag" "$file" --clobber
  done
}

notes() {
  retry gh release edit "$1" --notes-file "$2" >/dev/null
}

download() {
  local tag=$1 dir=$2 count
  mkdir -p "$dir"
  count=$(retry gh release view "$tag" --json assets --jq '.assets | length')
  if (( count == 0 )); then
    echo "Release $tag has no files yet"
    return 0
  fi
  retry gh release download "$tag" --dir "$dir" --clobber
}

command=${1:-}
shift || true
case "$command" in
  ensure) ensure_release "$@" ;;
  upload) upload "$@" ;;
  notes) notes "$@" ;;
  download) download "$@" ;;
  *) echo "Usage: release.sh ensure|upload|notes|download ..." >&2; exit 2 ;;
esac
