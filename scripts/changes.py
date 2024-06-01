import subprocess
import pyperclip


def get_uncommitted_changes(repository_path):
    # Get the unstaged changes
    unstaged_command = ["git", "-C", repository_path, "diff"]
    unstaged_result = subprocess.run(unstaged_command, stdout=subprocess.PIPE, text=True)

    if unstaged_result.returncode != 0:
        raise Exception("Error running git diff command")

    unstaged_changes = unstaged_result.stdout.strip()

    # Get the staged changes
    staged_command = ["git", "-C", repository_path, "diff", "--cached"]
    staged_result = subprocess.run(staged_command, stdout=subprocess.PIPE, text=True)

    if staged_result.returncode != 0:
        raise Exception("Error running git diff --cached command")

    staged_changes = staged_result.stdout.strip()

    return {
        'unstaged_changes': unstaged_changes,
        'staged_changes': staged_changes
    }


if __name__ == "__main__":
    repo_path = "../"
    uncommitted_changes = get_uncommitted_changes(repo_path)
    output = "Unstaged changes:\n"
    output += uncommitted_changes['unstaged_changes']
    # output += "\n\nStaged changes:\n"
    # output += uncommitted_changes['staged_changes']
    pyperclip.copy(output)
