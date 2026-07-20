```bash
#!/bin/bash

NEW_NAME="Mohamed Taleb"
NEW_EMAIL="mtaleb.contact@gmail.com"

git config user.name "$NEW_NAME"
git config user.email "$NEW_EMAIL"

export GIT_AUTHOR_NAME="$NEW_NAME"
export GIT_AUTHOR_EMAIL="$NEW_EMAIL"
export GIT_COMMITTER_NAME="$NEW_NAME"
export GIT_COMMITTER_EMAIL="$NEW_EMAIL"

git rebase --exec 'git commit --amend --no-edit --reset-author' HEAD~3

if [ $? -ne 0 ]; then
    echo "Rebase failed."
    echo "Fix any conflict, then run:"
    echo "git rebase --continue"
    exit 1
fi

echo
echo "Updated commits:"
git log -3 --format='Commit: %h%nAuthor: %an <%ae>%nCommitter: %cn <%ce>%n'

echo
echo "Push the rewritten commits with:"
echo "git push --force-with-lease origin main"
```
