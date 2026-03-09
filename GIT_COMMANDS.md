# How to Push to GitHub — Step by Step

## Prerequisites
- [Git installed](https://git-scm.com/downloads) on your computer
- A [GitHub account](https://github.com)

---

## Step 1 — Create the Repository on GitHub

1. Go to https://github.com/new
2. Repository name: `tinyml-mosquito-detector`
3. Description: `Real-time Aedes mosquito species classifier using TinyML on XIAO ESP32-S3`
4. Set to **Public**
5. Do NOT check "Add a README" (we already have one)
6. Click **Create repository**

---

## Step 2 — Initialize Git Locally

Open a terminal/command prompt and navigate to the project folder:

```bash
cd path/to/tinyml-mosquito-detector

# Initialize git
git init

# Add all files
git add .

# Make your first commit
git commit -m "Initial commit: Aedes mosquito wingbeat classifier"
```

---

## Step 3 — Connect to GitHub and Push

```bash
# Connect your local repo to GitHub (replace YOUR_USERNAME)
git remote add origin https://github.com/YOUR_USERNAME/tinyml-mosquito-detector.git

# Set main branch
git branch -M main

# Push to GitHub
git push -u origin main
```

---

## Step 4 — Add Results Screenshots

After adding your confusion matrix and other images to `docs/results/`:

```bash
git add docs/results/
git commit -m "Add model results and confusion matrix"
git push
```

---

## Common Git Commands for Future Updates

```bash
# Check what files have changed
git status

# Stage specific file
git add arduino/aedes_classifier/aedes_classifier.ino

# Stage all changes
git add .

# Commit with a message
git commit -m "Update confidence threshold to 0.75"

# Push to GitHub
git push
```

---

## Good Commit Message Examples

```
Add confusion matrix results
Fix LED blink pattern for albopictus
Update README with hardware setup photo
Increase mic gain from 8x to 12x
Add dataset source information
```
