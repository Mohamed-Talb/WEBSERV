#!/usr/bin/env bash

set -e

NEW_NAME="Mohamed Taleb"
NEW_EMAIL="mtaleb.contact@gmail.com"
BACKUP_BRANCH="backup-before-email-fix-$(date +%Y%m%d-%H%M%S)"

# Make sure this is a Git repository.
git rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
    echo "Error: run this script inside your Git repository."
    exit 1
}

# Do not rewrite commits when there are uncommitted changes.
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Error: you have uncommitted changes."
    echo "Commit or stash them first."
    exit 1
fi

# Make sure there are at least two commits.
if ! git rev-parse HEAD~2 >/dev/null 2>&1; then
    echo "Error: the repository does not have enough commits."
    exit 1
fi

echo "Creating backup branch: $BACKUP_BRANCH"
git branch "$BACKUP_BRANCH"

echo "Setting Git identity..."
git config user.name "$NEW_NAME"
git config user.email "$NEW_EMAIL"

echo "Rewriting the last two commits..."

git rebase HEAD~2 --exec \
    "git commit --amend --no-edit --author='$NEW_NAME <$NEW_EMAIL>'"

echo
echo "Done."
echo "The last two commits now use:"
echo "Name:  $NEW_NAME"
echo "Email: $NEW_EMAIL"
echo
echo "Backup branch: $BACKUP_BRANCH"
echo
git log -2 --format='Commit: %h%nAuthor: %an <%ae>%nCommitter: %cn <%ce>%nSubject: %s%n'
