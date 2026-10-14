# Publishing

This directory is a standalone source repository. Build outputs, local caches,
machine-specific logs and upstream JUCE sources are excluded.

1. Create an empty GitHub repository with the desired name and visibility.
   Do not add a second README or license during creation.
2. Review staged files with `git diff --cached --stat` and `git diff --cached`.
3. Set your own Git author identity if not already configured, then commit:

   ```sh
   git commit -m "Initial release of Live Link"
   git remote add origin https://github.com/YOUR_ACCOUNT/YOUR_REPOSITORY.git
   git push -u origin main
   ```

4. Enable private vulnerability reporting in repository settings.
5. Use GitHub Releases for Windows installers and IPA archives; state clearly
   whether each artifact is signed. Do not upload signing material or credentials.
6. Keep the early-release status, AGPL license and USB limitation visible.

No remote repository is configured and no publication has been performed.
