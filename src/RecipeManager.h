
#ifndef RECIPE_MANAGER_H
#define RECIPE_MANAGER_H

#include <vector>
#include "Types.h"

class RecipeManager {
public:
    RecipeManager();
    bool loadRecipes();
    bool saveRecipes();
    const std::vector<Recipe>& getRecipes() const;

private:
    std::vector<Recipe> _recipes;
};

#endif // RECIPE_MANAGER_H


