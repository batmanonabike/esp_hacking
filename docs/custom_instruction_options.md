# Custom Instruction Options for GitHub Copilot in VS Code

VS Code provides several methods to customize GitHub Copilot's behavior through custom instructions. These options help you get more consistent and tailored responses based on your specific coding practices and project requirements.

## Instruction Files

VS Code supports Markdown files that provide context and instructions to GitHub Copilot:

### Global Workspace Instructions

Create a central instruction file that applies to the entire workspace:

```markdown
.github/copilot-instructions.md
```

This file should contain general guidelines for code style, architecture principles, and project-specific considerations.

### Task-Specific Instructions

For more focused tasks, create `.instructions.md` files in relevant directories:

```
src/components/.instructions.md
```

These files can contain component-specific guidelines or patterns.

## Settings-Based Instructions

Define custom instructions directly in VS Code settings:

```json
{
  "github.copilot.chat.codeGeneration.instructions": "Follow our team's naming conventions: camelCase for variables, PascalCase for classes.",
  "github.copilot.chat.testGeneration.instructions": "Always use Jest and follow AAA pattern (Arrange-Act-Assert).",
  "github.copilot.chat.reviewSelection.instructions": "Focus on performance issues and potential security vulnerabilities.",
  "github.copilot.chat.commitMessageGeneration.instructions": "Follow conventional commits format.",
  "github.copilot.chat.pullRequestDescriptionGeneration.instructions": "Include links to related issues and summarize key changes."
}
```

## Enabling Instruction Files

To enable the instruction files feature:

```json
{
  "github.copilot.chat.codeGeneration.useInstructionFiles": "true"
}
```

## Prompt Files (Experimental Feature)

Create reusable prompts that can be shared with your team:

1. Enable the feature:
```json
{
  "chat.promptFiles": "true"
}
```

2. Create prompt files with the `.prompt.md` extension in the `.github/prompts` folder.

3. Use these prompts by typing `/` followed by the prompt file name in the Copilot chat input.

## Best Practices

- Keep instructions clear and concise
- Include specific examples of desired code patterns
- Update instructions as your project evolves
- Combine global and task-specific instructions for best results
- Test your instructions with sample prompts to ensure they produce the desired output

## Example Instruction File

```markdown
# Project Coding Standards

## Naming Conventions
- Use camelCase for variables and functions
- Use PascalCase for classes and components
- Use UPPER_SNAKE_CASE for constants

## Code Structure
- Maximum function length: 30 lines
- Follow single responsibility principle
- Prefer composition over inheritance

## Error Handling
- Always use try/catch for async operations
- Log detailed error messages
- Return standardized error responses

## Project-Specific Guidelines
- Use our custom state management pattern (see src/core/state.js)
- Follow accessibility guidelines for all UI components
- Implement lazy loading for all routes
```